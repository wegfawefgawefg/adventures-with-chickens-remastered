#pragma once

#include <optional>
#include <string>

struct State;

struct CliOptions {
    bool render_smoke{};
    bool report_performance{};
    bool explicit_window_size{};
    bool controls_preview{};
    std::string capture_path;
};

std::optional<int> dispatch_cli(State& game, int argc, char** argv, CliOptions& options);
