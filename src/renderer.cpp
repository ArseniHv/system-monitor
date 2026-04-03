#include "utils.h"
#include "renderer.h"
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/ioctl.h>
#include <unistd.h>

// ── ANSI codes ────────────────────────────────────────────────────

static const char* RESET  = "\033[0m";
static const char* GREEN  = "\033[32m";
static const char* YELLOW = "\033[33m";
static const char* RED    = "\033[31m";
static const char* BOLD   = "\033[1m";
static const char* CYAN   = "\033[36m";
//static const char* WHITE  = "\033[37m";

// Sparkline Unicode block characters.
static const char* SPARK_CHARS[] = {
    " ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"
};
static const int SPARK_LEVELS = 8;

// ── Constructor ───────────────────────────────────────────────────

Renderer::Renderer(const RenderConfig& cfg) : cfg_(cfg) {
    // Detect terminal width; fall back to 72 if unavailable.
    struct winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 20)
        box_width_ = std::min((int)ws.ws_col - 2, 78);
    else
        box_width_ = 70;
}

// ── Terminal control ──────────────────────────────────────────────

void Renderer::clear_screen() {
    std::cout << "\033[2J\033[H";
}

void Renderer::move_to(int row, int col) {
    std::cout << "\033[" << row << ";" << col << "H";
}

void Renderer::set_color(const std::string& code) {
    if (cfg_.use_color) std::cout << code;
}

void Renderer::reset_color() {
    if (cfg_.use_color) std::cout << RESET;
}

// ── Top-level render ──────────────────────────────────────────────

void Renderer::render(const SystemData& data) {
    clear_screen();
    int row = 1;
    render_header   (data, row);
    render_cpu      (data, row);
    render_memory   (data, row);
    render_network  (data, row);
    render_disk     (data, row);
    render_processes(data, row);
    render_footer   (row);
    std::cout.flush();
}

// ── Header ────────────────────────────────────────────────────────

void Renderer::render_header(const SystemData& d, int& row) {
    std::string uptime_str  = format_uptime(d.uptime_seconds);
    std::string title       = " SysPeek ";
    std::string right_label = " uptime: " + uptime_str + " ";

    // Top border with title and uptime.
    std::string top = "┌";
    top += title;
    int dashes = box_width_ - (int)title.size()
                            - (int)right_label.size() - 2;
    for (int i = 0; i < dashes; ++i) top += "─";
    top += right_label + "┐";

    move_to(row++, 1);
    set_color(BOLD);
    std::cout << top;
    reset_color();

    // Hostname + CPU model line.
    std::string info = " " + d.hostname + "  " + d.cpu.model_name;
    if ((int)info.size() > box_width_ - 2)
        info = info.substr(0, box_width_ - 5) + "...";
    move_to(row++, 1);
    std::cout << "│";
    set_color(CYAN);
    std::cout << pad_right(info, box_width_ - 2);
    reset_color();
    std::cout << "│";

    // Load average line.
    std::ostringstream load_ss;
    load_ss << std::fixed << std::setprecision(2)
            << " Load avg: " << d.load1
            << "  " << d.load5
            << "  " << d.load15 << "  (1m  5m  15m)";
    move_to(row++, 1);
    std::cout << "│" << pad_right(load_ss.str(), box_width_ - 2) << "│";

    // Separator.
    move_to(row++, 1);
    std::string sep = "├";
    for (int i = 0; i < box_width_ - 2; ++i) sep += "─";
    sep += "┤";
    std::cout << sep;
}

// ── CPU ───────────────────────────────────────────────────────────

void Renderer::render_cpu(const SystemData& d, int& row) {
    move_to(row++, 1);
    std::cout << "│";
    set_color(BOLD);
    std::cout << pad_right(" CPU Usage:", box_width_ - 2);
    reset_color();
    std::cout << "│";

    for (const auto& core : d.cpu.cores) {
        std::string bar   = progress_bar(core.usage_pct);
        std::string spark = sparkline(core.history);

        std::ostringstream line;
        line << std::fixed << std::setprecision(1);
        line << "  Core " << std::setw(2) << core.core_id << "  ["
             << bar << "]  ";

        // Color the percentage.
        std::string pct_str;
        {
            std::ostringstream ps;
            ps << std::fixed << std::setprecision(1)
               << core.usage_pct << "%";
            pct_str = ps.str();
        }

        std::string spark_padded = "  " + spark;
        std::string line_str = line.str()
                             + pad_left(pct_str, 6)
                             + spark_padded;
        line_str = pad_right(line_str, box_width_ - 2);

        move_to(row++, 1);
        std::cout << "│";
        // Print up to the percentage position in plain text, then color it.
        std::string prefix = "  Core ";
        std::ostringstream core_id_ss;
        core_id_ss << std::setw(2) << core.core_id;
        prefix += core_id_ss.str() + "  [";
        std::cout << prefix;
        set_color(color_for(core.usage_pct));
        std::cout << bar;
        reset_color();
        std::cout << "]  ";
        set_color(color_for(core.usage_pct));
        std::cout << pad_left(pct_str, 6);
        reset_color();
        std::cout << "  " << spark;

        // Pad to fill the box.
        // spark chars are multi-byte; approximate padding.
        int pad = box_width_ - 2 - 14 - cfg_.bar_width - 8 - (int)spark.size();
        for (int i = 0; i < pad; ++i) std::cout << ' ';
        std::cout << "│";
    }

    // Total CPU line.
    std::ostringstream total_line;
    total_line << std::fixed << std::setprecision(1);
    total_line << "  Total   [" << progress_bar(d.cpu.total_usage_pct)
               << "]  " << d.cpu.total_usage_pct << "%";

    move_to(row++, 1);
    std::cout << "│  Total   [";
    set_color(color_for(d.cpu.total_usage_pct));
    std::cout << progress_bar(d.cpu.total_usage_pct);
    reset_color();
    std::ostringstream total_pct;
    total_pct << std::fixed << std::setprecision(1)
              << d.cpu.total_usage_pct << "%";
    std::cout << "]  " << pad_left(total_pct.str(), 6);
    int fill = box_width_ - 2 - 11 - cfg_.bar_width - 4 - 6;
    for (int i = 0; i < fill; ++i) std::cout << ' ';
    std::cout << "│";

    // Separator.
    move_to(row++, 1);
    std::string sep = "├";
    for (int i = 0; i < box_width_ - 2; ++i) sep += "─";
    sep += "┤";
    std::cout << sep;
}

// ── Memory ────────────────────────────────────────────────────────

void Renderer::render_memory(const SystemData& d, int& row) {
    auto& m = d.memory;

    double ram_pct  = (m.total_kb > 0)
        ? (double)m.used_kb  / m.total_kb  * 100.0 : 0.0;
    double swap_pct = (m.swap_total_kb > 0)
        ? (double)m.swap_used_kb / m.swap_total_kb * 100.0 : 0.0;

    auto mem_line = [&](const std::string& label,
                        double pct,
                        long used_kb,
                        long total_kb) {
        std::ostringstream info;
        info << std::fixed << std::setprecision(1);
        info << "  " << label << "  ["
             << progress_bar(pct) << "]  "
             << format_kb(used_kb) << " / "
             << format_kb(total_kb);
        std::ostringstream pct_str;
        pct_str << std::fixed << std::setprecision(1) << "  (" << pct << "%)";

        std::string full = info.str() + pct_str.str();
        full = pad_right(full, box_width_ - 2);

        move_to(row++, 1);
        std::cout << "│  " << label << "  [";
        set_color(color_for(pct));
        std::cout << progress_bar(pct);
        reset_color();
        std::cout << "]  " << format_kb(used_kb)
                  << " / " << format_kb(total_kb);
        std::ostringstream pct_ss;
        pct_ss << std::fixed << std::setprecision(1) << "  (" << pct << "%)";
        std::cout << pct_ss.str();
        int printed = 2 + (int)label.size() + 4 + cfg_.bar_width
                    + 4 + (int)format_kb(used_kb).size()
                    + 3 + (int)format_kb(total_kb).size()
                    + (int)pct_ss.str().size();
        for (int i = printed; i < box_width_ - 2; ++i) std::cout << ' ';
        std::cout << "│";
    };

    mem_line("Memory:", ram_pct,  m.used_kb,      m.total_kb);
    mem_line("Swap:  ", swap_pct, m.swap_used_kb, m.swap_total_kb);

    move_to(row++, 1);
    std::string sep = "├";
    for (int i = 0; i < box_width_ - 2; ++i) sep += "─";
    sep += "┤";
    std::cout << sep;
}

// ── Network ───────────────────────────────────────────────────────

void Renderer::render_network(const SystemData& d, int& row) {
    move_to(row++, 1);
    std::ostringstream line;
    line << "  Network: " << d.network.interface
         << "   ↓ " << format_bytes(d.network.rx_bytes_per_sec)
         << "   ↑ " << format_bytes(d.network.tx_bytes_per_sec);
    std::string s = pad_right(line.str(), box_width_ - 2);
    std::cout << "│" << s << "│";
}

// ── Disk ───────────────────────────────────────────────

void Renderer::render_disk(const SystemData& d, int& row) {
    if (d.disk.device.empty()) return;

    move_to(row++, 1);
    std::ostringstream line;
    line << std::fixed << std::setprecision(2);
    line << "  Disk: " << d.disk.device
         << "   ↓ " << d.disk.read_mb_per_sec  << " MB/s"
         << "   ↑ " << d.disk.write_mb_per_sec << " MB/s";
    std::cout << "│" << pad_right(line.str(), box_width_ - 2) << "│";

    move_to(row++, 1);
    std::string sep = "├";
    for (int i = 0; i < box_width_ - 2; ++i) sep += "─";
    sep += "┤";
    std::cout << sep;
}

// ── Processes ─────────────────────────────────────────────────────

void Renderer::render_processes(const SystemData& d, int& row) {
    move_to(row++, 1);
    std::cout << "│";
    set_color(BOLD);
    std::cout << pad_right(" Top Processes:", box_width_ - 2);
    reset_color();
    std::cout << "│";

    // Header row.
    move_to(row++, 1);
    std::string hdr = "  " + pad_right("PID",  7)
                    + pad_right("NAME",  18)
                    + pad_right("CPU%",  8);
    std::cout << "│";
    set_color(BOLD);
    std::cout << pad_right(hdr, box_width_ - 2);
    reset_color();
    std::cout << "│";

    for (const auto& p : d.processes) {
        std::ostringstream line;
        line << "  "
             << pad_right(std::to_string(p.pid), 7)
             << pad_right(p.name, 18);
        std::ostringstream pct;
        pct << std::fixed << std::setprecision(1) << p.cpu_pct << "%";
        line << pad_right(pct.str(), 8);

        move_to(row++, 1);
        std::cout << "│" << pad_right(line.str(), box_width_ - 2) << "│";
    }
}

// ── Footer ────────────────────────────────────────────────────────

void Renderer::render_footer(int row) {
    move_to(row++, 1);
    std::string sep = "├";
    for (int i = 0; i < box_width_ - 2; ++i) sep += "─";
    sep += "┤";
    std::cout << sep;

    move_to(row++, 1);
    std::string hint = "  [q] quit   [r] refresh now";
    std::cout << "│" << pad_right(hint, box_width_ - 2) << "│";

    move_to(row++, 1);
    std::string bot = "└";
    for (int i = 0; i < box_width_ - 2; ++i) bot += "─";
    bot += "┘";
    set_color(BOLD);
    std::cout << bot;
    reset_color();
}

// ── Helpers ───────────────────────────────────────────────────────

std::string Renderer::progress_bar(double pct) const {
    if (pct < 0.0)   pct = 0.0;
    if (pct > 100.0) pct = 100.0;
    int filled = (int)(pct / 100.0 * cfg_.bar_width);
    std::string bar;
    bar.reserve(cfg_.bar_width * 3); // Unicode chars are 3 bytes each.
    for (int i = 0; i < cfg_.bar_width; ++i)
        bar += (i < filled) ? "█" : "░";
    return bar;
}

std::string Renderer::sparkline(const std::vector<double>& history) const {
    if (history.empty()) return "";
    std::string s;
    for (double v : history) {
        if (v < 0.0)   v = 0.0;
        if (v > 100.0) v = 100.0;
        int idx = (int)(v / 100.0 * SPARK_LEVELS);
        if (idx > SPARK_LEVELS) idx = SPARK_LEVELS;
        s += SPARK_CHARS[idx];
    }
    return s;
}

std::string Renderer::color_for(double pct) const {
    if (!cfg_.use_color) return "";
    if (pct >= 80.0) return RED;
    if (pct >= 50.0) return YELLOW;
    return GREEN;
}

std::string Renderer::format_bytes(double bps) const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    if (bps >= 1024.0 * 1024.0)
        ss << bps / (1024.0 * 1024.0) << " MB/s";
    else if (bps >= 1024.0)
        ss << bps / 1024.0 << " KB/s";
    else
        ss << bps << " B/s";
    return ss.str();
}

std::string Renderer::format_uptime(double seconds) const {
    int total = (int)seconds;
    int days  = total / 86400;
    int hours = (total % 86400) / 3600;
    int mins  = (total % 3600)  / 60;
    std::ostringstream ss;
    if (days  > 0) ss << days  << "d ";
    if (hours > 0) ss << hours << "h ";
    ss << mins << "m";
    return ss.str();
}

std::string Renderer::pad_right(const std::string& s, int width) const {
    if ((int)s.size() >= width) return s.substr(0, width);
    return s + std::string(width - (int)s.size(), ' ');
}

std::string Renderer::pad_left(const std::string& s, int width) const {
    if ((int)s.size() >= width) return s.substr(0, width);
    return std::string(width - (int)s.size(), ' ') + s;
}