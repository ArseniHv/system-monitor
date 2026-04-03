#include "collector.h"
#include <chrono>
#include <thread>

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

void Collector::run() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(refresh_ms_));
    }
}