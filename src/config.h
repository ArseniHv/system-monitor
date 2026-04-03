#pragma once
#include <string>

// Holds all runtime configuration.
// Populated from (in priority order):
//   1. command-line flags
//   2. ~/.config/system-monitor/config
//   3. built-in defaults
struct Config {
    int         refresh_ms  = 1000;
    bool        use_color   = true;
    int         bar_width   = 20;
    int         sparkline_len = 20;
    std::string network_iface = "";   // empty = auto-detect
    std::string disk_device   = "";   // empty = auto-detect
};

// Load config file; silently ignores missing file.
Config load_config_file();

// Merge command-line overrides into cfg (flags take priority).
void apply_cli_args(Config& cfg, int argc, char** argv);