#pragma once

#include "state.hpp"

struct InputState;
class Audio;

namespace step {

void step(State& game, const InputState& input, Audio* audio = nullptr);

} // namespace step
