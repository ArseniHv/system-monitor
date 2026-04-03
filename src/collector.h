#pragma once
#include "system_data.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct CpuRawTicks {
    long long user=0, nice=0, system=0, idle=0,
              iowait=0, irq=0, softirq=0, steal=0;

    long long active() const {
        return user + nice + system + irq + softirq + steal;
    }
    long long total() const {
        return active() + idle + iowait;
    }
};

struct NetRawCounters {
    long long rx_bytes = 0;
    long long tx_bytes = 0;
};

struct DiskRawCounters {
    long long read_sectors  = 0;
    long long write_sectors = 0;
};

class Collector {
public:
    explicit Collector(int refresh_ms);
    ~Collector();

    void start();
    void stop();

    std::mutex data_mutex;
    SystemData shared_data;

    // Wired up from Config in Part 5.
    std::string preferred_iface_;
    std::string preferred_disk_;
    int         sparkline_len_ = 20;

private:
    void run();

    // Reads current raw ticks from /proc/stat into out_ticks.
    // Returns false if /proc/stat is unreadable.
    bool read_cpu_ticks(std::vector<CpuRawTicks>& out_ticks);

    // Computes usage% from two tick snapshots, populates out_cores / out_total.
    void compute_cpu_usage(const std::vector<CpuRawTicks>& prev,
                           const std::vector<CpuRawTicks>& curr,
                           std::vector<CpuCoreData>& out_cores,
                           double& out_total);

    bool parse_memory  (MemoryData& out);
    bool parse_network (NetRawCounters& prev, NetworkData& out,
                        double elapsed_sec);
    bool parse_disk    (DiskRawCounters& prev, DiskData& out,
                        double elapsed_sec);
    bool parse_uptime  (double& out);
    bool parse_loadavg (double& l1, double& l5, double& l15);
    void parse_cpuinfo (std::string& model_name);
    void read_hostname (std::string& hostname);
    bool parse_processes(std::vector<ProcessInfo>& out,
                         long long total_delta,
                         int num_cpus);

    std::string detect_net_iface();
    std::string detect_disk_device();

    std::thread       thread_;
    std::atomic<bool> running_{false};
    int               refresh_ms_;
};