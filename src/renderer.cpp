#include "renderer.h"

Renderer::Renderer(const RenderConfig& cfg) : cfg_(cfg) {}

void Renderer::render(const SystemData&) {
    // Full implementation in Part 4.
}

std::string Renderer::progress_bar(double) const { return ""; }
std::string Renderer::color_for(double) const    { return ""; }
std::string Renderer::sparkline(const std::vector<double>&) const { return ""; }
std::string Renderer::format_bytes(double) const { return ""; }
std::string Renderer::format_uptime(double) const { return ""; }