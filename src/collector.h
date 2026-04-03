#pragma once
#include "system_data.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Raw per-core CPU tick counters — kept between readings to compute delta.
struct CpuRawTicks {
    long long user = 0, nice = 0, system = 0, idle = 0,
              iowait = 0, irq = 0, softirq = 0, steal = 0;

    long long active() const {
        return user + nice + system + irq + softirq + steal;
    }
    long long total() const {
        return active() + idle + iowait;
    }
};

// Raw network byte counters — kept between readings to compute delta.
struct NetRawCounters {
    long long rx_bytes = 0;
    long long tx_bytes = 0;
};

// Raw disk sector counters — kept between readings to compute delta.
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

    std::mutex  data_mutex;
    SystemData  shared_data;

private:
    void run();

    // /proc parsers — each returns true on success.
    bool parse_cpu(std::vector<CpuRawTicks>& prev_ticks,
                   std::vector<CpuCoreData>& out_cores,
                   double& out_total);
    bool parse_memory(MemoryData& out);
    bool parse_processes(std::vector<ProcessInfo>& out,
                         const std::vector<CpuRawTicks>& prev,
                         const std::vector<CpuRawTicks>& curr,
                         long long total_delta);
    bool parse_network(NetRawCounters& prev,
                       NetworkData& out,
                       double elapsed_sec);
    bool parse_disk(DiskRawCounters& prev,          
                    DiskData& out,
                    double elapsed_sec);
    bool parse_uptime(double& out);
    bool parse_loadavg(double& l1, double& l5, double& l15); 
    void parse_cpuinfo(std::string& model_name);             
    void read_hostname(std::string& hostname);              

    std::string detect_net_iface();
    std::string detect_disk_device();                       

    std::thread       thread_;
    std::atomic<bool> running_{false};
    int               refresh_ms_;

    // Configurable via Config (wired up in Part 5).
    std::string preferred_iface_;
    std::string preferred_disk_;
    int         sparkline_len_ = 20;   
};