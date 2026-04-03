#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include "collector.h"
#include "config.h"
#include "renderer.h"

int main(int argc, char** argv) {
    Config cfg = load_config_file();
    apply_cli_args(cfg, argc, argv);

    Collector collector(cfg.refresh_ms);
    collector.start();

    // Wait two cycles so the collector has a real delta.
    std::this_thread::sleep_for(
        std::chrono::milliseconds(cfg.refresh_ms * 2));

    SystemData snap;
    {
        std::lock_guard<std::mutex> lk(collector.data_mutex);
        snap = collector.shared_data;
    }
    collector.stop();

    RenderConfig rcfg;
    rcfg.use_color     = cfg.use_color;
    rcfg.bar_width     = cfg.bar_width;
    rcfg.sparkline_len = cfg.sparkline_len;

    Renderer renderer(rcfg);
    renderer.render(snap);

    // Park cursor below the box.
    std::cout << "\n";
    return 0;
}