#pragma once
#include "system_data.h"
#include <atomic>
#include <mutex>
#include <thread>

class Collector {
public:
    explicit Collector(int refresh_ms);
    ~Collector();

    void start();
    void stop();

    // Caller must hold the mutex while reading shared_data.
    std::mutex   data_mutex;
    SystemData   shared_data;

private:
    void run();

    std::thread          thread_;
    std::atomic<bool>    running_{false};
    int                  refresh_ms_;
};