#pragma once

#include "math.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

constexpr int level_count = 30;
constexpr int level_width = 240;
constexpr int level_height = 132;
constexpr int level_cell_count = level_width * level_height;

struct Warp {
    IVec2 position;
    IVec2 destination;
};

struct Level {
    std::array<std::int8_t, level_cell_count> tiles{};
    std::array<std::int8_t, level_cell_count> tube_underlay{};
    IVec2 player_start;
    IVec2 exit;
    std::vector<IVec2> chickens;
    std::vector<IVec2> aliens;
    std::vector<Warp> warps;
};

bool load_levels(const std::filesystem::path& path, std::vector<Level>& levels, std::string& error);
