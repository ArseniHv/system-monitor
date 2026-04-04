#pragma once
#include "system_data.h"
#include <string>
#include <vector>

struct RenderConfig {
    bool use_color    = true;
    int  bar_width    = 20;
    int  sparkline_len = 20;
    int  refresh_ms    = 1000;
};

class Renderer {
public:
    explicit Renderer(const RenderConfig& cfg);
    void render(const SystemData& data);

private:
    RenderConfig cfg_;

    // Terminal control
    void clear_screen();
    void move_to(int row, int col);
    void set_color(const std::string& code);
    void reset_color();

    // Section renderers
    void render_header    (const SystemData& d, int& row);
    void render_cpu       (const SystemData& d, int& row);
    void render_memory    (const SystemData& d, int& row);
    void render_network   (const SystemData& d, int& row);
    void render_disk      (const SystemData& d, int& row);
    void render_processes (const SystemData& d, int& row);
    void render_footer    (int row);

    // Helpers
    std::string progress_bar   (double pct) const;
    std::string sparkline      (const std::vector<double>& history) const;
    std::string color_for      (double pct) const;
    std::string format_bytes   (double bytes_per_sec) const;
    std::string format_uptime  (double seconds) const;
    std::string pad_right      (const std::string& s, int width) const;
    std::string pad_left       (const std::string& s, int width) const;

    // Box width — determined once at construction from terminal size.
    int box_width_ = 70;
};