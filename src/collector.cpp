#include "collector.h"
#include "utils.h"
#include <algorithm>
#include <chrono>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <thread>
#include <unistd.h>

// ── Constructor / lifecycle ───────────────────────────────────────

Collector::Collector(int refresh_ms) : refresh_ms_(refresh_ms) {}
Collector::~Collector() { stop(); }

void Collector::start() {
    running_ = true;
    thread_  = std::thread(&Collector::run, this);
}

void Collector::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

// ── Background thread ─────────────────────────────────────────────

void Collector::run() {
    std::vector<CpuRawTicks> prev_ticks;
    NetRawCounters            prev_net{};
    DiskRawCounters           prev_disk{};

    // First snapshot — gives us a baseline for deltas.
    read_cpu_ticks(prev_ticks);
    {
        NetworkData dummy_net;
        DiskData    dummy_disk;
        parse_network(prev_net,  dummy_net,  1.0);
        parse_disk   (prev_disk, dummy_disk, 1.0);
    }

    auto last_time = std::chrono::steady_clock::now();

    while (running_) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(refresh_ms_));

        auto   now         = std::chrono::steady_clock::now();
        double elapsed_sec = std::chrono::duration<double>(
                                 now - last_time).count();
        last_time = now;

        SystemData fresh{};

        // ── CPU ───────────────────────────────────────────────────
        std::vector<CpuRawTicks> curr_ticks;
        read_cpu_ticks(curr_ticks);
        compute_cpu_usage(prev_ticks, curr_ticks,
                          fresh.cpu.cores, fresh.cpu.total_usage_pct);

        // Carry forward sparkline history (add-on C).
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            for (size_t i = 0;
                 i < fresh.cpu.cores.size() &&
                 i < shared_data.cpu.cores.size(); ++i) {
                fresh.cpu.cores[i].history =
                    shared_data.cpu.cores[i].history;
            }
        }
        for (auto& core : fresh.cpu.cores) {
            core.history.push_back(core.usage_pct);
            if ((int)core.history.size() > sparkline_len_)
                core.history.erase(core.history.begin());
        }

        // ── Other metrics ─────────────────────────────────────────
        parse_memory  (fresh.memory);
        parse_network (prev_net,  fresh.network, elapsed_sec);
        parse_disk    (prev_disk, fresh.disk,    elapsed_sec);
        parse_uptime  (fresh.uptime_seconds);
        parse_loadavg (fresh.load1, fresh.load5, fresh.load15);
        parse_cpuinfo (fresh.cpu.model_name);
        read_hostname (fresh.hostname);

        // ── Processes ─────────────────────────────────────────────
        long long total_delta = 0;
        int num_cpus = 1;
        if (curr_ticks.size() > 1 && prev_ticks.size() == curr_ticks.size()) {
            total_delta = curr_ticks[0].total() - prev_ticks[0].total();
            num_cpus    = static_cast<int>(curr_ticks.size()) - 1;
        }
        parse_processes(fresh.processes, total_delta, num_cpus);

        prev_ticks = curr_ticks;

        // ── Publish ───────────────────────────────────────────────
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            shared_data = fresh;
        }
    }
}

// ── CPU: read raw ticks ───────────────────────────────────────────

bool Collector::read_cpu_ticks(std::vector<CpuRawTicks>& out_ticks) {
    std::ifstream f("/proc/stat");
    if (!f.is_open()) return false;

    out_ticks.clear();
    std::string line;
    bool aggregate_seen = false;

    while (std::getline(f, line)) {
        if (line.rfind("cpu", 0) != 0) break; // cpu lines are at the top

        std::istringstream ss(line);
        std::string label;
        ss >> label;

        CpuRawTicks t;
        ss >> t.user >> t.nice >> t.system >> t.idle
           >> t.iowait >> t.irq >> t.softirq >> t.steal;

        if (label == "cpu") {
            // Aggregate line always goes at index 0.
            out_ticks.insert(out_ticks.begin(), t);
            aggregate_seen = true;
        } else {
            out_ticks.push_back(t); // per-core: index 1+
        }
    }
    return aggregate_seen;
}

// ── CPU: compute usage from delta ────────────────────────────────

void Collector::compute_cpu_usage(const std::vector<CpuRawTicks>& prev,
                                   const std::vector<CpuRawTicks>& curr,
                                   std::vector<CpuCoreData>& out_cores,
                                   double& out_total) {
    out_cores.clear();
    out_total = 0.0;

    if (prev.empty() || curr.empty() || prev.size() != curr.size())
        return;

    // Index 0 is aggregate.
    {
        long long ad = curr[0].active() - prev[0].active();
        long long td = curr[0].total()  - prev[0].total();
        out_total = (td > 0) ? (double)ad / td * 100.0 : 0.0;
    }

    // Index 1+ are per-core.
    for (size_t i = 1; i < curr.size(); ++i) {
        long long ad = curr[i].active() - prev[i].active();
        long long td = curr[i].total()  - prev[i].total();
        CpuCoreData c;
        c.core_id   = static_cast<int>(i) - 1;
        c.usage_pct = (td > 0) ? (double)ad / td * 100.0 : 0.0;
        out_cores.push_back(c);
    }
}

// ── /proc/meminfo ─────────────────────────────────────────────────

bool Collector::parse_memory(MemoryData& out) {
    std::ifstream f("/proc/meminfo");
    if (!f.is_open()) return false;

    long mem_total=0, mem_free=0, mem_buffers=0,
         mem_cached=0, swap_total=0, swap_free=0;

    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string key; long value;
        ss >> key >> value;
        if      (key == "MemTotal:")   mem_total   = value;
        else if (key == "MemFree:")    mem_free    = value;
        else if (key == "Buffers:")    mem_buffers = value;
        else if (key == "Cached:")     mem_cached  = value;
        else if (key == "SwapTotal:")  swap_total  = value;
        else if (key == "SwapFree:")   swap_free   = value;
    }

    out.total_kb      = mem_total;
    out.used_kb       = mem_total - mem_free - mem_buffers - mem_cached;
    out.swap_total_kb = swap_total;
    out.swap_used_kb  = swap_total - swap_free;
    return true;
}

// ── /proc/net/dev ─────────────────────────────────────────────────

std::string Collector::detect_net_iface() {
    if (!preferred_iface_.empty()) return preferred_iface_;
    std::ifstream f("/proc/net/dev");
    if (!f.is_open()) return "";
    std::string line;
    std::getline(f, line);
    std::getline(f, line);
    while (std::getline(f, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string iface = trim(line.substr(0, colon));
        if (iface != "lo") return iface;
    }
    return "";
}

bool Collector::parse_network(NetRawCounters& prev,
                               NetworkData& out,
                               double elapsed_sec) {
    std::string iface = detect_net_iface();
    if (iface.empty()) return false;

    std::ifstream f("/proc/net/dev");
    if (!f.is_open()) return false;

    std::string line;
    std::getline(f, line);
    std::getline(f, line);

    while (std::getline(f, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        if (trim(line.substr(0, colon)) != iface) continue;

        std::istringstream ss(line.substr(colon + 1));
        long long rx, tx, dummy;
        ss >> rx >> dummy >> dummy >> dummy
           >> dummy >> dummy >> dummy >> dummy >> tx;

        if (elapsed_sec > 0.0) {
            out.rx_bytes_per_sec =
                std::max(0.0, (double)(rx - prev.rx_bytes) / elapsed_sec);
            out.tx_bytes_per_sec =
                std::max(0.0, (double)(tx - prev.tx_bytes) / elapsed_sec);
        }
        out.interface = iface;
        prev = {rx, tx};
        return true;
    }
    return false;
}

// ── /proc/diskstats (add-on A) ────────────────────────────────────

std::string Collector::detect_disk_device() {
    if (!preferred_disk_.empty()) return preferred_disk_;
    std::ifstream f("/proc/diskstats");
    if (!f.is_open()) return "";
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        int major, minor;
        std::string name;
        ss >> major >> minor >> name;
        if (name.find("loop") != std::string::npos) continue;
        if (std::isdigit((unsigned char)name.back())) continue;
        return name;
    }
    return "";
}

bool Collector::parse_disk(DiskRawCounters& prev,
                            DiskData& out,
                            double elapsed_sec) {
    std::string dev = detect_disk_device();
    if (dev.empty()) return false;

    std::ifstream f("/proc/diskstats");
    if (!f.is_open()) return false;

    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        int major, minor;
        std::string name;
        ss >> major >> minor >> name;
        if (name != dev) continue;

        long long dummy, read_sectors, write_sectors;
        ss >> dummy >> dummy >> read_sectors >> dummy
           >> dummy >> dummy >> write_sectors;

        if (elapsed_sec > 0.0) {
            double rb = (double)(read_sectors  - prev.read_sectors)  * 512.0;
            double wb = (double)(write_sectors - prev.write_sectors) * 512.0;
            out.read_mb_per_sec  = std::max(0.0, rb / elapsed_sec / (1024.0*1024.0));
            out.write_mb_per_sec = std::max(0.0, wb / elapsed_sec / (1024.0*1024.0));
        }
        out.device = dev;
        prev = {read_sectors, write_sectors};
        return true;
    }
    return false;
}

// ── /proc/[pid]/stat ─────────────────────────────────────────────

bool Collector::parse_processes(std::vector<ProcessInfo>& out,
                                 long long total_delta,
                                 int num_cpus) {
    out.clear();
    if (total_delta <= 0) return false;

    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) return false;

    struct dirent* entry;
    while ((entry = readdir(proc_dir)) != nullptr) {
        std::string dname(entry->d_name);
        if (dname.empty() || !std::isdigit((unsigned char)dname[0])) continue;

        std::ifstream stat_f("/proc/" + dname + "/stat");
        if (!stat_f.is_open()) continue;

        std::string stat_line;
        std::getline(stat_f, stat_line);

        auto paren_open  = stat_line.find('(');
        auto paren_close = stat_line.rfind(')');
        if (paren_open  == std::string::npos ||
            paren_close == std::string::npos) continue;

        std::string name = stat_line.substr(
            paren_open + 1, paren_close - paren_open - 1);

        std::istringstream ss(stat_line.substr(paren_close + 2));
        std::vector<std::string> fields;
        std::string field;
        while (ss >> field) fields.push_back(field);

        // Fields (after state): ppid(1) pgrp(2) ... utime(11) stime(12)
        if (fields.size() < 13) continue;

        long long utime = std::stoll(fields[11]);
        long long stime = std::stoll(fields[12]);
        double cpu_pct  =
            (double)(utime + stime) / total_delta * 100.0 * num_cpus;

        ProcessInfo p;
        p.pid     = std::stoi(dname);
        p.name    = name;
        p.cpu_pct = std::max(0.0, std::min(cpu_pct, 100.0 * num_cpus));
        out.push_back(p);
    }
    closedir(proc_dir);

    std::sort(out.begin(), out.end(),
              [](const ProcessInfo& a, const ProcessInfo& b) {
                  return a.cpu_pct > b.cpu_pct;
              });
    if (out.size() > 10) out.resize(10);
    return true;
}

// ── /proc/uptime ──────────────────────────────────────────────────

bool Collector::parse_uptime(double& out) {
    std::ifstream f("/proc/uptime");
    if (!f.is_open()) return false;
    f >> out;
    return true;
}

// ── /proc/loadavg ──────────────────────────────────────

bool Collector::parse_loadavg(double& l1, double& l5, double& l15) {
    std::ifstream f("/proc/loadavg");
    if (!f.is_open()) return false;
    f >> l1 >> l5 >> l15;
    return true;
}

// ── /proc/cpuinfo ──────────────────────────────────────

void Collector::parse_cpuinfo(std::string& model_name) {
    if (!model_name.empty()) return;
    std::ifstream f("/proc/cpuinfo");
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("model name", 0) == 0) {
            auto colon = line.find(':');
            if (colon != std::string::npos)
                model_name = trim(line.substr(colon + 1));
            return;
        }
    }
    model_name = "Unknown CPU";
}

// ── hostname ───────────────────────────────────────────

void Collector::read_hostname(std::string& hostname) {
    if (!hostname.empty()) return;
    char buf[256] = {};
    if (gethostname(buf, sizeof(buf)) == 0)
        hostname = buf;
}