#include "config.h"
#include "utils.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/stat.h>

// ── Config file path ──────────────────────────────────────────────

static std::string config_path() {
    const char* home = std::getenv("HOME");
    if (!home) return "";
    return std::string(home) + "/.config/system-monitor/config";
}

// ── Load config file ───────────────────────────────────
// File format (one key=value per line, # for comments):
//
//   refresh_ms=1000
//   use_color=true
//   bar_width=20
//   sparkline_len=20
//   network_iface=eth0
//   disk_device=sda

Config load_config_file() {
    Config cfg;
    std::string path = config_path();
    if (path.empty()) return cfg;

    std::ifstream f(path);
    if (!f.is_open()) return cfg; // missing file is fine — use defaults

    std::string line;
    while (std::getline(f, line)) {
        // Strip comments and blank lines.
        auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        line = trim(line);
        if (line.empty()) continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        if (key.empty() || val.empty()) continue;

        if      (key == "refresh_ms")    cfg.refresh_ms    = std::stoi(val);
        else if (key == "bar_width")     cfg.bar_width     = std::stoi(val);
        else if (key == "sparkline_len") cfg.sparkline_len = std::stoi(val);
        else if (key == "network_iface") cfg.network_iface = val;
        else if (key == "disk_device")   cfg.disk_device   = val;
        else if (key == "use_color") {
            std::string v = val;
            std::transform(v.begin(), v.end(), v.begin(), ::tolower);
            cfg.use_color = (v == "true" || v == "1" || v == "yes");
        }
    }

    // Clamp values to sane ranges.
    if (cfg.refresh_ms    <  100) cfg.refresh_ms    = 100;
    if (cfg.refresh_ms    > 60000) cfg.refresh_ms   = 60000;
    if (cfg.bar_width     <  5)   cfg.bar_width     = 5;
    if (cfg.bar_width     >  50)  cfg.bar_width     = 50;
    if (cfg.sparkline_len <  5)   cfg.sparkline_len = 5;
    if (cfg.sparkline_len >  60)  cfg.sparkline_len = 60;

    return cfg;
}

// ── Write default config ──────────────────────────────

void write_default_config() {
    std::string path = config_path();
    if (path.empty()) return;

    // Create directory if needed.
    std::string dir = path.substr(0, path.rfind('/'));
    mkdir(dir.c_str(), 0755);

    // Don't overwrite an existing config.
    std::ifstream check(path);
    if (check.is_open()) return;

    std::ofstream f(path);
    if (!f.is_open()) return;

    f << "# SysPeek configuration file\n"
      << "# Generated automatically — edit as needed.\n"
      << "#\n"
      << "# refresh_ms    — how often to update (milliseconds, min 100)\n"
      << "# use_color     — enable ANSI colors (true/false)\n"
      << "# bar_width     — width of progress bars (5–50)\n"
      << "# sparkline_len — number of history samples in sparkline (5–60)\n"
      << "# network_iface — force a specific network interface (e.g. eth0)\n"
      << "# disk_device   — force a specific disk device (e.g. sda)\n"
      << "#\n"
      << "refresh_ms=1000\n"
      << "use_color=true\n"
      << "bar_width=20\n"
      << "sparkline_len=20\n"
      << "# network_iface=eth0\n"
      << "# disk_device=sda\n";
}

// ── CLI argument parsing ──────────────────────────────────────────

void apply_cli_args(Config& cfg, int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);

        if (arg == "--help" || arg == "-h") {
            std::cout <<
                "Usage: system-monitor [OPTIONS]\n\n"
                "Options:\n"
                "  --refresh N     Refresh interval in milliseconds (default: 1000)\n"
                "  --no-color      Disable ANSI color output\n"
                "  --bar-width N   Progress bar width in characters (default: 20)\n"
                "  --iface NAME    Network interface to monitor (e.g. eth0)\n"
                "  --disk NAME     Disk device to monitor (e.g. sda)\n"
                "  --version       Print version and exit\n"
                "  --help          Show this help message\n\n"
                "Config file: ~/.config/system-monitor/config\n";
            std::exit(0);
        }

        if (arg == "--version" || arg == "-v") {
            std::cout << "syspeek " << cfg.version << "\n";
            std::exit(0);
        }

        if (arg == "--no-color") {
            cfg.use_color = false;
            continue;
        }

        // Flags that take a value: --flag value or --flag=value
        auto get_val = [&](const std::string& flag) -> std::string {
            std::string prefix = flag + "=";
            if (arg.rfind(prefix, 0) == 0)
                return arg.substr(prefix.size());
            if (arg == flag && i + 1 < argc)
                return std::string(argv[++i]);
            return "";
        };

        std::string val;

        val = get_val("--refresh");
        if (!val.empty()) {
            cfg.refresh_ms = std::stoi(val);
            if (cfg.refresh_ms < 100) cfg.refresh_ms = 100;
            continue;
        }

        val = get_val("--bar-width");
        if (!val.empty()) {
            cfg.bar_width = std::stoi(val);
            continue;
        }

        val = get_val("--iface");
        if (!val.empty()) {
            cfg.network_iface = val;
            continue;
        }

        val = get_val("--disk");
        if (!val.empty()) {
            cfg.disk_device = val;
            continue;
        }
    }
}