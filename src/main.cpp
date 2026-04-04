#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <mutex>
#include <termios.h>
#include <thread>
#include <unistd.h>

#include "collector.h"
#include "config.h"
#include "renderer.h"

// ── Signal handling ───────────────────────────────────────────────

static std::atomic<bool> g_running{true};

static void on_signal(int) {
    g_running = false;
}

// ── Non-blocking keyboard input ───────────────────────────────────

static termios g_orig_termios;

static void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    termios raw = g_orig_termios;
    // Disable canonical mode and echo so we can read single keypresses.
    raw.c_lflag    &= ~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 0; // non-blocking read
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
}

// Returns the character pressed, or 0 if no key is waiting.
static char poll_key() {
    char c = 0;
    if (read(STDIN_FILENO, &c, 1) == 1) return c;
    return 0;
}

// ── Main ──────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    // Load config: file first, then CLI overrides.
    Config cfg = load_config_file();
    apply_cli_args(cfg, argc, argv);

    // Write a default config file if none exists.
    write_default_config();

    // Wire config into collector.
    Collector collector(cfg.refresh_ms);
    collector.preferred_iface_  = cfg.network_iface;
    collector.preferred_disk_   = cfg.disk_device;
    collector.sparkline_len_    = cfg.sparkline_len;

    RenderConfig rcfg;
    rcfg.use_color     = cfg.use_color;
    rcfg.bar_width     = cfg.bar_width;
    rcfg.sparkline_len = cfg.sparkline_len;
     rcfg.refresh_ms    = cfg.refresh_ms;
    Renderer renderer(rcfg);

    // Set up signal handlers for clean exit.
    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);

    // Hide the cursor while the UI is running.
    if (cfg.use_color) std::cout << "\033[?25l";

    enable_raw_mode();
    collector.start();

    bool force_refresh = false;

    while (g_running) {
        // Check for keypress.
        char key = poll_key();
        if (key == 'q' || key == 'Q') break;
        if (key == 'r' || key == 'R') force_refresh = true;

        // Copy shared data under the lock.
        SystemData snap;
        {
            std::lock_guard<std::mutex> lk(collector.data_mutex);
            snap = collector.shared_data;
        }

        renderer.render(snap);

        if (force_refresh) {
            force_refresh = false;
            continue; // Don't sleep — render immediately again next loop.
        }

        // Sleep in small increments so keypress feels responsive.
        int slept_ms = 0;
        while (g_running && slept_ms < cfg.refresh_ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            slept_ms += 50;
            key = poll_key();
            if (key == 'q' || key == 'Q') { g_running = false; break; }
            if (key == 'r' || key == 'R') { force_refresh = true;  break; }
        }
    }

    // ── Clean shutdown ────────────────────────────────────────────
    collector.stop();
    disable_raw_mode();

    // Restore cursor and clear screen.
    if (cfg.use_color) std::cout << "\033[?25h";
    std::cout << "\033[2J\033[H" << std::flush;
    std::cout << "syspeek exited cleanly.\n";

    return 0;
}