#include "audio.hpp"
#include "state.hpp"

#include <algorithm>
#include <emscripten/emscripten.h>
#include <memory>

namespace {

// clang-format off
EM_JS(void, init_web_audio, (), {
    if (globalThis.__awcAudio)
        return;
    globalThis.__awcAudio = {
        unlocked : false,
        master : 1.0,
        effects : 1.0,
        music : 0.8,
        voices : [],
        musicElement : null,
    };
});

EM_JS(void, shutdown_web_audio, (), {
    const state = globalThis.__awcAudio;
    if (!state)
        return;
    for (const voice of state.voices)
        voice.pause();
    if (state.musicElement)
        state.musicElement.pause();
    globalThis.__awcAudio = null;
});

EM_JS(void, play_web_sound, (int sound), {
    const filenames = [
        "Alienhit.wav", "Applause.wav", "AtticDoor.wav", "Bounce.wav",    "Broken.wav",
        "Chicken.wav",  "Chicken2.wav", "Clink.wav",     "Clunk.wav",     "Down.wav",
        "Dribble.wav",  "Drink.wav",    "Error.wav",     "Excellent.wav", "Explode.wav",
        "Harp.wav",     "Horn.wav",     "Intro.wav",     "Move.wav",      "OpenDoor.wav",
        "Pickup.wav",   "Rev.wav",      "Warp.wav",      "Whistle.wav",   "XGames.wav",
        "Yeah.wav",
    ];
    const state = globalThis.__awcAudio;
    const filename = filenames[sound];
    if (!state || !filename || !state.unlocked)
        return;
    const voice = new Audio(new URL("audio/sounds/" + filename, document.baseURI));
    voice.volume = Math.max(0, Math.min(1, state.master * state.effects));
    state.voices = state.voices.filter((candidate) => !candidate.ended);
    if (state.voices.length >= 16) {
        state.voices[0].pause();
        state.voices.shift();
    }
    state.voices.push(voice);
    void voice.play().catch(() => {});
});

EM_JS(int, web_sound_is_playing, (int sound), {
    const filenames = [
        "Alienhit.wav", "Applause.wav", "AtticDoor.wav", "Bounce.wav",    "Broken.wav",
        "Chicken.wav",  "Chicken2.wav", "Clink.wav",     "Clunk.wav",     "Down.wav",
        "Dribble.wav",  "Drink.wav",    "Error.wav",     "Excellent.wav", "Explode.wav",
        "Harp.wav",     "Horn.wav",     "Intro.wav",     "Move.wav",      "OpenDoor.wav",
        "Pickup.wav",   "Rev.wav",      "Warp.wav",      "Whistle.wav",   "XGames.wav",
        "Yeah.wav",
    ];
    const filename = filenames[sound];
    const state = globalThis.__awcAudio;
    if (!state || !filename)
        return 0;
    return state.voices.some((voice) => !voice.ended && !voice.paused &&
                                       decodeURIComponent(voice.src).endsWith(filename))
               ? 1
               : 0;
});

EM_JS(void, play_web_music, (int music), {
    const filenames = [
        null,
        "End.ogg",
        "Jamin.ogg",
        "Organjam.ogg",
        "Rock1.ogg",
        "Rock2.ogg",
        "Run.ogg",
        "Sweet.ogg",
    ];
    const state = globalThis.__awcAudio;
    if (!state)
        return;
    if (state.musicElement) {
        state.musicElement.pause();
        state.musicElement = null;
    }
    const filename = filenames[music];
    if (!filename)
        return;
    const element = new Audio(new URL("audio/music/" + filename, document.baseURI));
    element.loop = music !== 1;
    element.volume = Math.max(0, Math.min(1, state.master * state.music));
    state.musicElement = element;
    if (state.unlocked)
        void element.play().catch(() => {});
});

EM_JS(void, set_web_audio_volume, (int channel, double volume), {
    const state = globalThis.__awcAudio;
    if (!state)
        return;
    const value = Math.max(0, Math.min(1, volume));
    if (channel === 0)
        state.master = value;
    else if (channel === 1)
        state.effects = value;
    else
        state.music = value;
    for (const voice of state.voices)
        voice.volume = Math.max(0, Math.min(1, state.master * state.effects));
    if (state.musicElement)
        state.musicElement.volume = Math.max(0, Math.min(1, state.master * state.music));
});
// clang-format on

} // namespace

struct Audio::Impl {
    Music music{Music::none};
};

Audio::Audio() : impl_(std::make_unique<Impl>()) {
}

Audio::~Audio() {
    shutdown_web_audio();
}

bool Audio::open() {
    init_web_audio();
    return true;
}

void Audio::play(Sound sound) {
    play_web_sound(static_cast<int>(sound));
}

bool Audio::is_playing(Sound sound) const {
    return web_sound_is_playing(static_cast<int>(sound)) != 0;
}

void Audio::play_music(Music music) {
    if (music == impl_->music) {
        return;
    }
    impl_->music = music;
    play_web_music(static_cast<int>(music));
}

void Audio::stop_music() {
    play_music(Music::none);
}

Music Audio::current_music() const {
    return impl_->music;
}

void Audio::set_master_volume(float volume) {
    set_web_audio_volume(0, static_cast<double>(std::clamp(volume, 0.0F, 1.0F)));
}

void Audio::set_effect_volume(float volume) {
    set_web_audio_volume(1, static_cast<double>(std::clamp(volume, 0.0F, 1.0F)));
}

void Audio::set_music_volume(float volume) {
    set_web_audio_volume(2, static_cast<double>(std::clamp(volume, 0.0F, 1.0F)));
}

Music music_for_state(const State& state) {
    switch (state.mode) {
    case Mode::startup:
        return Music::none;
    case Mode::title:
    case Mode::player_select:
    case Mode::instructions:
    case Mode::options:
    case Mode::order_info:
    case Mode::credits:
        return Music::rock2;
    case Mode::playing:
    case Mode::results:
        switch ((state.current_level - 1) % 5) {
        case 0:
            return Music::run;
        case 1:
            return Music::organjam;
        case 2:
            return Music::sweet;
        case 3:
            return Music::jamin;
        case 4:
            return Music::rock1;
        default:
            return Music::none;
        }
    case Mode::congratulations:
        return state.congratulations_mode == CongratulationsMode::active ? Music::end : Music::none;
    case Mode::exiting:
        return Music::rock2;
    }
    return Music::none;
}

void sync_music(Audio& audio, const State& state) {
    audio.play_music(state.save.music ? music_for_state(state) : Music::none);
}
