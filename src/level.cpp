#include "level.hpp"

#include <algorithm>
#include <fstream>
#include <initializer_list>
#include <iterator>

namespace {

constexpr std::array<std::uint8_t, 16> level_signature{
    'J', 'e', 's', 'u', 's', ' ', 'i', 's', ' ', 'm', 'y', ' ', 'k', 'i', 'n', 'g',
};
constexpr std::size_t offset_table_start = level_signature.size();
constexpr std::size_t offset_size = 3;

IVec2 point_for_cell(std::size_t cell) {
    return {
        .x = static_cast<int>(cell % static_cast<std::size_t>(level_width)),
        .y = static_cast<int>(cell / static_cast<std::size_t>(level_width)),
    };
}

std::size_t big_endian_offset(const std::vector<std::uint8_t>& bytes, std::size_t position) {
    return (static_cast<std::size_t>(bytes[position]) << 16U) |
           (static_cast<std::size_t>(bytes[position + 1]) << 8U) |
           static_cast<std::size_t>(bytes[position + 2]);
}

bool one_of(int tile, std::initializer_list<int> values) {
    return std::ranges::find(values, tile) != values.end();
}

bool repair_level_15_route(Level& level) {
    // fix level 15 warps
    const auto exit_return = std::ranges::find(level.warps, IVec2{79, 51}, &Warp::position);
    const auto maze_link = std::ranges::find(level.warps, IVec2{146, 96}, &Warp::position);
    if (exit_return == level.warps.end() || maze_link == level.warps.end()) {
        return false;
    }

    constexpr IVec2 authored_exit_return{9, 96};
    constexpr IVec2 authored_maze_link{150, 25};
    constexpr IVec2 repaired_exit_return{150, 25};
    constexpr IVec2 repaired_maze_link{9, 96};
    if (exit_return->destination == authored_exit_return &&
        maze_link->destination == authored_maze_link) {
        std::swap(exit_return->destination, maze_link->destination);
    }
    return exit_return->destination == repaired_exit_return &&
           maze_link->destination == repaired_maze_link;
}

void normalize_runtime_tubes(Level& level) {
    // mark tube crossings
    const auto authored_tiles = level.tiles;
    for (std::size_t index = 0; index < authored_tiles.size(); ++index) {
        const int tile = authored_tiles[index];
        const int x = static_cast<int>(index % static_cast<std::size_t>(level_width));
        const int y = static_cast<int>(index / static_cast<std::size_t>(level_width));

        // connect tube segments
        if (tile == 20) {
            const bool joins_from_left =
                x > 0 && one_of(authored_tiles[index - 1], {21, 22, 23, 29, 33, 34});
            const bool joins_from_right =
                x + 1 < level_width && one_of(authored_tiles[index + 1], {21, 24, 25, 27, 31, 34});
            if (joins_from_left || joins_from_right) {
                level.tiles[index] = 34;
            }
            continue;
        }
        if (tile == 21) {
            const bool joins_from_above =
                y > 0 && one_of(authored_tiles[index - level_width], {20, 23, 24, 26, 30, 34});
            const bool joins_from_below =
                y + 1 < level_height &&
                one_of(authored_tiles[index + level_width], {20, 22, 25, 28, 32, 34});
            if (joins_from_above || joins_from_below) {
                level.tiles[index] = 34;
            }
            continue;
        }

        // calc crossing direction
        int step = 0;
        int remaining = 0;
        int empty_replacement = 0;
        if (tile == 30) {
            if (y == 0 || level.tube_underlay[index - level_width] == 1) {
                continue;
            }
            step = -level_width;
            remaining = y;
            empty_replacement = 1;
        } else if (tile == 31) {
            step = 1;
            remaining = level_width - x - 1;
            empty_replacement = 2;
        } else if (tile == 32) {
            step = level_width;
            remaining = level_height - y - 1;
            empty_replacement = 1;
        } else if (tile == 33) {
            if (x == 0 || level.tube_underlay[index - 1] == 2) {
                continue;
            }
            step = -1;
            remaining = x;
            empty_replacement = 2;
        } else {
            continue;
        }

        // fill crossing underlay
        std::size_t target = static_cast<std::size_t>(static_cast<int>(index) + step);
        for (int distance = 0; distance < remaining; ++distance) {
            const int authored_target = authored_tiles[target];
            if (authored_target >= 30 && authored_target <= 33) {
                break;
            }
            level.tube_underlay[target] =
                static_cast<std::int8_t>(level.tube_underlay[target] == 0 ? empty_replacement : 3);
            if (distance + 1 < remaining) {
                target = static_cast<std::size_t>(static_cast<int>(target) + step);
            }
        }
    }
}

bool decode_level(const std::vector<std::uint8_t>& bytes, std::size_t start, std::size_t end,
                  Level& level, std::string& error) {
    // decode level
    std::size_t input = start;
    std::size_t output = 0;
    bool found_player = false;
    bool found_exit = false;

    while (output < level.tiles.size()) {
        if (input >= end) {
            error = "compressed level ended before its terrain was complete";
            return false;
        }

        // decode warp
        const std::uint8_t control = bytes[input++];
        const int tile = static_cast<int>(control & 0x7fU) - 3;
        if (tile == 8) {
            if (input + 2 > end) {
                error = "warp record is truncated";
                return false;
            }
            const IVec2 position = point_for_cell(output);
            level.tiles[output++] = static_cast<std::int8_t>(tile);
            level.warps.push_back({
                .position = position,
                .destination = {bytes[input], bytes[input + 1]},
            });
            input += 2;
            continue;
        }

        // decode run length
        if (input >= end) {
            error = "run-length record is truncated";
            return false;
        }
        const std::size_t count =
            (static_cast<std::size_t>(control & 0xc0U) << 2U) + bytes[input++];
        if (output + count > level.tiles.size()) {
            error = "run-length record exceeds the 240x132 terrain";
            return false;
        }

        // record level objects
        for (std::size_t repeat = 0; repeat < count; ++repeat) {
            const IVec2 point = point_for_cell(output);
            level.tiles[output++] = static_cast<std::int8_t>(tile);
            switch (tile) {
            case -3:
                level.aliens.push_back(point);
                break;
            case -2:
                level.chickens.push_back(point);
                break;
            case -1:
                level.player_start = point;
                found_player = true;
                level.tiles[output - 1] = 0;
                break;
            case 9:
                level.exit = point;
                found_exit = true;
                break;
            default:
                break;
            }
        }
    }

    // validate level
    if (input != end) {
        error = "compressed level did not end at the next indexed offset";
        return false;
    }
    if (!found_player || !found_exit) {
        error = "level is missing its player start or exit";
        return false;
    }
    normalize_runtime_tubes(level);
    return true;
}

} // namespace

bool load_levels(const std::filesystem::path& path, std::vector<Level>& levels,
                 std::string& error) {
    std::ifstream file{path, std::ios::binary};
    if (!file) {
        error = "could not open " + path.string();
        return false;
    }

    // validate file header
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>{file},
                                          std::istreambuf_iterator<char>{}};
    const std::size_t table_end =
        offset_table_start + static_cast<std::size_t>(level_count) * offset_size;
    if (bytes.size() < table_end ||
        !std::equal(level_signature.begin(), level_signature.end(), bytes.begin())) {
        error = "Levels.dat signature or offset table is invalid";
        return false;
    }

    // calc level ranges
    std::array<std::size_t, level_count + 1> offsets{};
    for (int index = 0; index < level_count; ++index) {
        const std::size_t position =
            offset_table_start + static_cast<std::size_t>(index) * offset_size;
        offsets[static_cast<std::size_t>(index)] = big_endian_offset(bytes, position);
    }
    offsets.back() = bytes.size();

    // decode all levels
    levels.clear();
    levels.reserve(level_count);
    for (int index = 0; index < level_count; ++index) {
        const std::size_t start = offsets[static_cast<std::size_t>(index)];
        const std::size_t end = offsets[static_cast<std::size_t>(index + 1)];
        if (start < table_end || start >= end || end > bytes.size()) {
            error = "Levels.dat contains an invalid level offset";
            levels.clear();
            return false;
        }

        Level level;
        if (!decode_level(bytes, start, end, level, error)) {
            error = "level " + std::to_string(index + 1) + ": " + error;
            levels.clear();
            return false;
        }
        if (index == 14 && !repair_level_15_route(level)) {
            error = "level 15 does not contain the known authored warp route";
            levels.clear();
            return false;
        }
        levels.push_back(std::move(level));
    }
    return true;
}
