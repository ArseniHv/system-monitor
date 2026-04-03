#pragma once
#include <string>
#include <vector>

struct CpuCoreData {
    int    core_id    = 0;
    double usage_pct  = 0.0;
    std::vector<double> history; 
};

struct CpuData {
    std::string              model_name; 
    std::vector<CpuCoreData> cores;
    double                   total_usage_pct = 0.0;
};

struct MemoryData {
    long total_kb      = 0;
    long used_kb       = 0;
    long swap_total_kb = 0;
    long swap_used_kb  = 0;
};

struct ProcessInfo {
    int         pid     = 0;
    std::string name;
    double      cpu_pct = 0.0;
};

struct NetworkData {
    std::string interface;
    double rx_bytes_per_sec = 0.0;
    double tx_bytes_per_sec = 0.0;
};

struct DiskData {          
    std::string device;
    double read_mb_per_sec  = 0.0;
    double write_mb_per_sec = 0.0;
};

struct SystemData {
    CpuData                  cpu;
    MemoryData               memory;
    NetworkData              network;
    DiskData                 disk;
    std::vector<ProcessInfo> processes;
    double                   uptime_seconds = 0.0;
    std::string              hostname;     
    double load1  = 0.0;                  
    double load5  = 0.0;
    double load15 = 0.0;
};