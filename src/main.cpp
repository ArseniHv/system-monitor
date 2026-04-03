#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include "collector.h"
#include "config.h"

int main(int argc, char** argv) {
    Config cfg = load_config_file();
    apply_cli_args(cfg, argc, argv);

    Collector collector(cfg.refresh_ms);
    collector.start();

    // Wait two refresh cycles so the collector has a real delta.
    std::this_thread::sleep_for(
        std::chrono::milliseconds(cfg.refresh_ms * 2));

    // Read and print a single snapshot then exit.
    SystemData snap;
    {
        std::lock_guard<std::mutex> lk(collector.data_mutex);
        snap = collector.shared_data;
    }

    collector.stop();

    std::cout << "=== System Monitor — collector smoke test ===\n";
    std::cout << "Hostname : " << snap.hostname << "\n";
    std::cout << "CPU      : " << snap.cpu.model_name << "\n";
    std::cout << "Uptime   : " << (int)(snap.uptime_seconds / 3600)
              << "h " << (int)(snap.uptime_seconds / 60) % 60 << "m\n";
    std::cout << "Load avg : " << snap.load1 << "  "
              << snap.load5 << "  " << snap.load15 << "\n";
    std::cout << "CPU total: " << snap.cpu.total_usage_pct << "%\n";
    for (auto& c : snap.cpu.cores)
        std::cout << "  Core " << c.core_id << ": " << c.usage_pct << "%\n";
    std::cout << "RAM used : " << snap.memory.used_kb / 1024
              << " MB / " << snap.memory.total_kb / 1024 << " MB\n";
    std::cout << "Network  : " << snap.network.interface
              << "  rx " << snap.network.rx_bytes_per_sec / 1024.0 << " KB/s"
              << "  tx " << snap.network.tx_bytes_per_sec / 1024.0 << " KB/s\n";
    std::cout << "Disk     : " << snap.disk.device
              << "  r " << snap.disk.read_mb_per_sec << " MB/s"
              << "  w " << snap.disk.write_mb_per_sec << " MB/s\n";
    std::cout << "Top processes:\n";
    for (auto& p : snap.processes)
        std::cout << "  [" << p.pid << "] " << p.name
                  << "  " << p.cpu_pct << "%\n";

    return 0;
}