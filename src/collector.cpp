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
    NetRawCounters           prev_net{};
    DiskRawCounters          prev_disk{};
    bool                     first = true;

    // Pre-populate previous readings so the first real reading has a delta.
    {
        std::vector<CpuCoreData> dummy_cores;
        double dummy_total = 0.0;
        parse_cpu(prev_ticks, dummy_cores, dummy_total);
        parse_network(prev_net, shared_data.network,  1.0);
        parse_disk   (prev_disk, shared_data.disk,    1.0);
    }

    auto last_time = std::chrono::steady_clock::now();

    while (running_) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(refresh_ms_));

        auto   now          = std::chrono::steady_clock::now();
        double elapsed_sec  = std::chrono::duration<double>(
                                  now - last_time).count();
        last_time = now;

        // ── Collect all metrics into local variables ──────────────
        SystemData fresh{};

        std::vector<CpuRawTicks> curr_ticks;
        parse_cpu(curr_ticks, fresh.cpu.cores, fresh.cpu.total_usage_pct);

        // Restore sparkline history from shared_data (add-on C).
        if (!first) {
            std::lock_guard<std::mutex> lk(data_mutex);
            for (size_t i = 0;
                 i < fresh.cpu.cores.size() &&
                 i < shared_data.cpu.cores.size(); ++i) {
                fresh.cpu.cores[i].history =
                    shared_data.cpu.cores[i].history;
            }
        }
        // Append current reading to each core's sparkline history.
        for (auto& core : fresh.cpu.cores) {
            core.history.push_back(core.usage_pct);
            if ((int)core.history.size() > sparkline_len_)
                core.history.erase(core.history.begin());
        }

        parse_memory  (fresh.memory);
        parse_network (prev_net,  fresh.network, elapsed_sec);
        parse_disk    (prev_disk, fresh.disk,    elapsed_sec);
        parse_uptime  (fresh.uptime_seconds);
        parse_loadavg (fresh.load1, fresh.load5, fresh.load15);
        parse_cpuinfo (fresh.cpu.model_name);
        read_hostname (fresh.hostname);

        // Process list needs raw tick delta for per-process CPU%.
        // We pass prev and curr ticks; total_delta is the aggregate delta.
        long long total_delta = 0;
        if (!prev_ticks.empty() && !curr_ticks.empty()) {
            total_delta = curr_ticks[0].total() - prev_ticks[0].total();
        }
        parse_processes(fresh.processes, prev_ticks, curr_ticks, total_delta);

        prev_ticks = curr_ticks;
        first = false;

        // ── Write to shared data ──────────────────────────────────
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            shared_data = fresh;
        }
    }
}

// ── /proc/stat — CPU ticks ────────────────────────────────────────

bool Collector::parse_cpu(std::vector<CpuRawTicks>& prev_ticks,
                           std::vector<CpuCoreData>& out_cores,
                           double& out_total) {
    std::ifstream f("/proc/stat");
    if (!f.is_open()) return false;

    std::vector<CpuRawTicks> curr_ticks;
    std::string line;

    while (std::getline(f, line)) {
        if (line.rfind("cpu", 0) != 0) continue;
        // Skip the aggregate "cpu " line for per-core, handle separately.
        std::istringstream ss(line);
        std::string label;
        ss >> label;

        CpuRawTicks t;
        ss >> t.user >> t.nice >> t.system >> t.idle
           >> t.iowait >> t.irq >> t.softirq >> t.steal;

        if (label == "cpu") {
            // Aggregate line — used for total usage.
            if (!prev_ticks.empty() && !prev_ticks[0].total() == 0) {
                // index 0 reserved for aggregate in prev_ticks
            }
            curr_ticks.insert(curr_ticks.begin(), t); // index 0 = aggregate
        } else {
            curr_ticks.push_back(t); // index 1+ = per-core
        }
    }

    if (curr_ticks.empty()) return false;

    // Calculate usage percentages using delta from previous reading.
    if (!prev_ticks.empty() && prev_ticks.size() == curr_ticks.size()) {
        // Total (index 0).
        long long active_delta = curr_ticks[0].active() - prev_ticks[0].active();
        long long total_delta  = curr_ticks[0].total()  - prev_ticks[0].total();
        out_total = (total_delta > 0)
            ? static_cast<double>(active_delta) / total_delta * 100.0
            : 0.0;

        // Per-core (index 1+).
        out_cores.clear();
        for (size_t i = 1; i < curr_ticks.size(); ++i) {
            long long ad = curr_ticks[i].active() - prev_ticks[i].active();
            long long td = curr_ticks[i].total()  - prev_ticks[i].total();
            CpuCoreData c;
            c.core_id   = static_cast<int>(i) - 1;
            c.usage_pct = (td > 0)
                ? static_cast<double>(ad) / td * 100.0
                : 0.0;
            out_cores.push_back(c);
        }
    } else {
        // First reading — no delta yet, emit zeroes.
        out_total = 0.0;
        out_cores.clear();
        for (size_t i = 1; i < curr_ticks.size(); ++i) {
            CpuCoreData c;
            c.core_id   = static_cast<int>(i) - 1;
            c.usage_pct = 0.0;
            out_cores.push_back(c);
        }
    }

    prev_ticks = curr_ticks;
    return true;
}

// ── /proc/meminfo ─────────────────────────────────────────────────

bool Collector::parse_memory(MemoryData& out) {
    std::ifstream f("/proc/meminfo");
    if (!f.is_open()) return false;

    long mem_total = 0, mem_free = 0, mem_buffers = 0, mem_cached = 0;
    long swap_total = 0, swap_free = 0;

    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string key;
        long value;
        ss >> key >> value;

        if      (key == "MemTotal:")    mem_total    = value;
        else if (key == "MemFree:")     mem_free     = value;
        else if (key == "Buffers:")     mem_buffers  = value;
        else if (key == "Cached:")      mem_cached   = value;
        else if (key == "SwapTotal:")   swap_total   = value;
        else if (key == "SwapFree:")    swap_free    = value;
    }

    out.total_kb      = mem_total;
    out.used_kb       = mem_total - mem_free - mem_buffers - mem_cached;
    out.swap_total_kb = swap_total;
    out.swap_used_kb  = swap_total - swap_free;
    return true;
}

// ── /proc/net/dev — network I/O ───────────────────────────────────

std::string Collector::detect_net_iface() {
    if (!preferred_iface_.empty()) return preferred_iface_;

    std::ifstream f("/proc/net/dev");
    if (!f.is_open()) return "";

    std::string line;
    std::getline(f, line); // header line 1
    std::getline(f, line); // header line 2

    while (std::getline(f, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string iface = trim(line.substr(0, colon));
        if (iface != "lo") return iface; // return first non-loopback
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
        long long rx, tx;
        long long dummy;
        ss >> rx >> dummy >> dummy >> dummy
           >> dummy >> dummy >> dummy >> dummy
           >> tx;

        NetRawCounters curr{rx, tx};
        if (elapsed_sec > 0.0) {
            out.rx_bytes_per_sec =
                static_cast<double>(curr.rx_bytes - prev.rx_bytes) / elapsed_sec;
            out.tx_bytes_per_sec =
                static_cast<double>(curr.tx_bytes - prev.tx_bytes) / elapsed_sec;
        }
        // Clamp negatives (counter wrap or first read).
        if (out.rx_bytes_per_sec < 0.0) out.rx_bytes_per_sec = 0.0;
        if (out.tx_bytes_per_sec < 0.0) out.tx_bytes_per_sec = 0.0;

        out.interface = iface;
        prev = curr;
        return true;
    }
    return false;
}

// ── /proc/diskstats — disk I/O (add-on A) ────────────────────────

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
        // Skip partitions (sdaX, nvme0n1pX) — want whole disks only.
        if (name.find("loop") != std::string::npos) continue;
        if (std::isdigit(name.back())) continue;
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

        long long dummy;
        long long reads_completed, read_sectors;
        long long writes_completed, write_sectors;

        ss >> reads_completed >> dummy >> read_sectors >> dummy
           >> writes_completed >> dummy >> write_sectors;

        DiskRawCounters curr{read_sectors, write_sectors};

        if (elapsed_sec > 0.0) {
            // Each sector is 512 bytes.
            double read_bytes =
                static_cast<double>(curr.read_sectors - prev.read_sectors)
                * 512.0;
            double write_bytes =
                static_cast<double>(curr.write_sectors - prev.write_sectors)
                * 512.0;
            out.read_mb_per_sec  = read_bytes  / elapsed_sec / (1024.0 * 1024.0);
            out.write_mb_per_sec = write_bytes / elapsed_sec / (1024.0 * 1024.0);
        }
        if (out.read_mb_per_sec  < 0.0) out.read_mb_per_sec  = 0.0;
        if (out.write_mb_per_sec < 0.0) out.write_mb_per_sec = 0.0;

        out.device = dev;
        prev = curr;
        return true;
    }
    return false;
}

// ── /proc/[pid]/stat — process list ──────────────────────────────

bool Collector::parse_processes(std::vector<ProcessInfo>& out,
                                 const std::vector<CpuRawTicks>& prev_ticks,
                                 const std::vector<CpuRawTicks>& curr_ticks,
                                 long long total_delta) {
    out.clear();
    if (total_delta <= 0) return false;

    int num_cpus = static_cast<int>(curr_ticks.size()) - 1;
    if (num_cpus < 1) num_cpus = 1;

    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) return false;

    struct dirent* entry;
    while ((entry = readdir(proc_dir)) != nullptr) {
        // Only process numeric directory names (PIDs).
        std::string dname(entry->d_name);
        if (dname.empty() || !std::isdigit(dname[0])) continue;
        int pid = std::stoi(dname);

        // Read /proc/[pid]/stat.
        std::string stat_path = "/proc/" + dname + "/stat";
        std::ifstream stat_f(stat_path);
        if (!stat_f.is_open()) continue;

        std::string stat_line;
        std::getline(stat_f, stat_line);

        // Process name is inside parentheses — handle spaces in names.
        auto paren_open  = stat_line.find('(');
        auto paren_close = stat_line.rfind(')');
        if (paren_open == std::string::npos ||
            paren_close == std::string::npos) continue;

        std::string name = stat_line.substr(
            paren_open + 1, paren_close - paren_open - 1);

        // Fields after ')': state, ppid, ... utime(14), stime(15).
        std::string rest = stat_line.substr(paren_close + 2);
        std::istringstream ss(rest);
        std::string field;
        std::vector<std::string> fields;
        while (ss >> field) fields.push_back(field);

        // utime = fields[11], stime = fields[12] (0-indexed after state).
        if (fields.size() < 13) continue;

        long long utime = std::stoll(fields[11]);
        long long stime = std::stoll(fields[12]);
        long long proc_total = utime + stime;

        // We don't track per-process previous ticks here — CPU% is
        // approximated as proc_time_in_interval / total_cpu_time * num_cpus.
        // This matches how most simple monitors work.
        double cpu_pct =
            static_cast<double>(proc_total) / total_delta * 100.0 * num_cpus;
        // Clamp to sane range.
        if (cpu_pct < 0.0)   cpu_pct = 0.0;
        if (cpu_pct > 100.0 * num_cpus) cpu_pct = 100.0 * num_cpus;

        ProcessInfo p;
        p.pid     = pid;
        p.name    = name;
        p.cpu_pct = cpu_pct;
        out.push_back(p);
    }
    closedir(proc_dir);

    // Sort descending by CPU usage, keep top 10.
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
    if (!model_name.empty()) return; // Only read once.
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
}

// ── hostname ───────────────────────────────────────────

void Collector::read_hostname(std::string& hostname) {
    if (!hostname.empty()) return; // Only read once.
    char buf[256] = {};
    if (gethostname(buf, sizeof(buf)) == 0)
        hostname = buf;
}