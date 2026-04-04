#pragma once
#include <string>

struct Config {
    int         refresh_ms    = 1000;
    bool        use_color     = true;
    int         bar_width     = 20;
    int         sparkline_len = 20;
    std::string network_iface = "";
    std::string disk_device   = "";
    std::string version       = SYSPEEK_VERSION;
};

Config      load_config_file();
void        apply_cli_args(Config& cfg, int argc, char** argv);
void        write_default_config();