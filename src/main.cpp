#include <iostream>
#include "config.h"

int main(int argc, char** argv) {
    Config cfg = load_config_file();
    apply_cli_args(cfg, argc, argv);

    std::cout << "System Monitor starting...\n";
    std::cout << "  refresh: " << cfg.refresh_ms << " ms\n";
    std::cout << "  color:   " << (cfg.use_color ? "on" : "off") << "\n";
    return 0;
}