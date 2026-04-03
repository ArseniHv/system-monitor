#pragma once
#include "system_data.h"

struct RenderConfig {
    bool  use_color   = true;   // --no-color disables ANSI colours
    int   bar_width   = 20;     // width of progress bars in characters
    int   sparkline_len = 20;   // history length for sparklines
};

class Renderer {
public:
    explicit Renderer(const RenderConfig& cfg);

    void render(const SystemData& data);

private:
    RenderConfig cfg_;

    std::string progress_bar(double pct) const;
    std::string color_for(double pct) const;
    std::string sparkline(const std::vector<double>& history) const; 
    std::string format_bytes(double bytes_per_sec) const;
    std::string format_uptime(double seconds) const;
};