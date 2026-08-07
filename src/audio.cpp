#include "audio.hpp"

#include "state.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fluidsynth.h>
#include <ranges>
#include <string>
#include <vector>

#ifndef AWC_ASSET_ROOT
#define AWC_ASSET_ROOT "assets"
#endif

namespace {

constexpr int sample_rate = 48000;
constexpr int channel_count = 2;
constexpr int maximum_voices = 16;
constexpr std::size_t sound_count = 26;
constexpr float dribble_gain = 4.0F;
static_assert(static_cast<std::size_t>(Sound::yeah) + 1 == sound_count);

constexpr std::array<const char*, sound_count> sound_filenames{{
    "Alienhit.wav", "Applause.wav",  "AtticDoor.wav", "Bounce.wav", "Broken.wav",  "Chicken.wav",
    "Chicken2.wav", "Clink.wav",     "Clunk.wav",     "Down.wav",   "Dribble.wav", "Drink.wav",
    "Error.wav",    "Excellent.wav", "Explode.wav",   "Harp.wav",   "Horn.wav",    "Intro.wav",
    "Move.wav",     "OpenDoor.wav",  "Pickup.wav",    "Rev.wav",    "Warp.wav",    "Whistle.wav",
    "XGames.wav",   "Yeah.wav",
}};

const char* music_filename(Music music) {
    switch (music) {
    case Music::none:
        return nullptr;
    case Music::end:
        return "End.mid";
    case Music::jamin:
        return "Jamin.mid";
    case Music::organjam:
        return "Organjam.mid";
    case Music::rock1:
        return "Rock1.mid";
    case Music::rock2:
        return "Rock2.mid";
    case Music::run:
        return "Run.mid";
    case Music::sweet:
        return "Sweet.mid";
    }
    return nullptr;
}

struct SoundData {
    std::vector<float> samples;
    float gain{1.0F};
};

struct Voice {
    const SoundData* sound{};
    std::size_t sample{};
};

} // namespace

struct Audio::Impl {
    SDL_AudioStream* stream{};
    SDL_Mutex* mutex{};
    fluid_settings_t* fluid_settings{};
    fluid_synth_t* synth{};
    fluid_player_t* player{};
    std::array<SoundData, sound_count> sounds;
    std::array<Voice, maximum_voices> voices;
    std::vector<float> mix_buffer;
    Music music{Music::none};
    float master_volume{1.0F};
    float effect_volume{1.0F};
    float music_volume{0.8F};

    static void SDLCALL fill_audio(void* userdata, SDL_AudioStream* audio_stream,
                                   int additional_amount, int) {
        auto& audio = *static_cast<Impl*>(userdata);
        const int bytes_per_frame = static_cast<int>(sizeof(float)) * channel_count;
        const int frames = (additional_amount + bytes_per_frame - 1) / bytes_per_frame;
        if (frames <= 0) {
            return;
        }

        // mix MIDI
        audio.mix_buffer.assign(static_cast<std::size_t>(frames * channel_count), 0.0F);
        SDL_LockMutex(audio.mutex);
        if (audio.synth != nullptr) {
            (void)fluid_synth_set_gain(audio.synth, audio.music_volume * audio.master_volume);
            (void)fluid_synth_write_float(audio.synth, frames, audio.mix_buffer.data(), 0, 2,
                                          audio.mix_buffer.data(), 1, 2);
        }

        // mix sound effects
        for (Voice& voice : audio.voices) {
            if (voice.sound == nullptr) {
                continue;
            }
            const std::size_t available = voice.sound->samples.size() - voice.sample;
            const std::size_t requested = audio.mix_buffer.size();
            const std::size_t count = std::min(available, requested);
            for (std::size_t index = 0; index < count; ++index) {
                audio.mix_buffer[index] += voice.sound->samples[voice.sample + index] *
                                           voice.sound->gain * audio.effect_volume *
                                           audio.master_volume;
            }
            voice.sample += count;
            if (voice.sample >= voice.sound->samples.size()) {
                voice = {};
            }
        }
        SDL_UnlockMutex(audio.mutex);

        // submit audio
        (void)SDL_PutAudioStreamData(audio_stream, audio.mix_buffer.data(),
                                     frames * bytes_per_frame);
    }

    bool load_sound(std::size_t index) {
        // load sound effect
        const std::filesystem::path path =
            std::filesystem::path{AWC_ASSET_ROOT} / "sounds" / sound_filenames[index];
        SDL_AudioSpec source_specification{};
        Uint8* source_data = nullptr;
        Uint32 source_size = 0;
        if (!SDL_LoadWAV(path.string().c_str(), &source_specification, &source_data,
                         &source_size)) {
            std::fprintf(stderr, "could not load %s: %s\n", path.string().c_str(), SDL_GetError());
            return false;
        }

        const SDL_AudioSpec destination_specification{
            .format = SDL_AUDIO_F32,
            .channels = channel_count,
            .freq = sample_rate,
        };
        Uint8* destination_data = nullptr;
        int destination_size = 0;
        const bool converted = SDL_ConvertAudioSamples(
            &source_specification, source_data, static_cast<int>(source_size),
            &destination_specification, &destination_data, &destination_size);
        SDL_free(source_data);
        if (!converted) {
            std::fprintf(stderr, "could not convert %s: %s\n", path.string().c_str(),
                         SDL_GetError());
            return false;
        }

        // store sound effect
        const auto* first = reinterpret_cast<const float*>(destination_data);
        const std::size_t count = static_cast<std::size_t>(destination_size) / sizeof(float);
        sounds[index].samples.assign(first, first + count);
        if (index == static_cast<std::size_t>(Sound::dribble)) {
            sounds[index].gain = dribble_gain;
        }
        SDL_free(destination_data);
        return true;
    }

    void delete_player() {
        if (player == nullptr) {
            return;
        }

        // stop MIDI player
        (void)fluid_player_stop(player);
        (void)fluid_player_join(player);
        delete_fluid_player(player);
        player = nullptr;
        if (synth != nullptr) {
            (void)fluid_synth_system_reset(synth);
        }
    }

    void close() {
        // stop audio callback
        if (stream != nullptr) {
            (void)SDL_PauseAudioStreamDevice(stream);
        }
        if (mutex != nullptr) {
            SDL_LockMutex(mutex);
        }
        delete_player();
        if (synth != nullptr) {
            delete_fluid_synth(synth);
            synth = nullptr;
        }
        if (fluid_settings != nullptr) {
            delete_fluid_settings(fluid_settings);
            fluid_settings = nullptr;
        }
        if (mutex != nullptr) {
            SDL_UnlockMutex(mutex);
        }

        // free audio resources
        SDL_DestroyAudioStream(stream);
        stream = nullptr;
        SDL_DestroyMutex(mutex);
        mutex = nullptr;
    }
};

Audio::Audio() : impl_(std::make_unique<Impl>()) {
}

Audio::~Audio() {
    impl_->close();
}

bool Audio::open() {
    // init MIDI synth
    impl_->mutex = SDL_CreateMutex();
    if (impl_->mutex == nullptr) {
        return false;
    }

    impl_->fluid_settings = new_fluid_settings();
    if (impl_->fluid_settings == nullptr) {
        return false;
    }
    (void)fluid_settings_setnum(impl_->fluid_settings, "synth.sample-rate",
                                static_cast<double>(sample_rate));
    impl_->synth = new_fluid_synth(impl_->fluid_settings);
    if (impl_->synth == nullptr) {
        return false;
    }

    // load soundfont
    const std::array<const char*, 2> soundfonts{{
        "/usr/share/sounds/sf2/TimGM6mb.sf2",
        "/usr/share/sounds/sf2/default-GM.sf2",
    }};
    bool loaded_soundfont = false;
    for (const char* path : soundfonts) {
        if (std::filesystem::exists(path) && fluid_synth_sfload(impl_->synth, path, 1) >= 0) {
            loaded_soundfont = true;
            break;
        }
    }
    if (!loaded_soundfont) {
        std::fprintf(stderr, "could not find a General MIDI SoundFont\n");
        return false;
    }

    // load sound effects
    for (std::size_t index = 0; index < impl_->sounds.size(); ++index) {
        if (!impl_->load_sound(index)) {
            return false;
        }
    }

    // start audio stream
    const SDL_AudioSpec specification{
        .format = SDL_AUDIO_F32,
        .channels = channel_count,
        .freq = sample_rate,
    };
    impl_->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &specification,
                                              Impl::fill_audio, impl_.get());
    return impl_->stream != nullptr && SDL_ResumeAudioStreamDevice(impl_->stream);
}

void Audio::play(Sound sound) {
    const std::size_t index = static_cast<std::size_t>(sound);
    if (index >= impl_->sounds.size() || impl_->mutex == nullptr) {
        return;
    }

    // assign sound voice
    SDL_LockMutex(impl_->mutex);
    Voice* selected = &impl_->voices[0];
    for (Voice& voice : impl_->voices) {
        if (voice.sound == nullptr) {
            selected = &voice;
            break;
        }
    }
    *selected = {.sound = &impl_->sounds[index], .sample = 0};
    SDL_UnlockMutex(impl_->mutex);
}

bool Audio::is_playing(Sound sound) const {
    const std::size_t index = static_cast<std::size_t>(sound);
    if (index >= impl_->sounds.size() || impl_->mutex == nullptr) {
        return false;
    }

    SDL_LockMutex(impl_->mutex);
    const SoundData* data = &impl_->sounds[index];
    const bool playing = std::ranges::any_of(
        impl_->voices, [data](const Voice& voice) { return voice.sound == data; });
    SDL_UnlockMutex(impl_->mutex);
    return playing;
}

void Audio::play_music(Music music) {
    if (music == impl_->music || impl_->synth == nullptr || impl_->mutex == nullptr) {
        return;
    }

    // switch music track
    SDL_LockMutex(impl_->mutex);
    impl_->delete_player();
    impl_->music = music;
    const char* filename = music_filename(music);
    if (filename != nullptr) {
        const std::filesystem::path path =
            std::filesystem::path{AWC_ASSET_ROOT} / "music" / filename;
        impl_->player = new_fluid_player(impl_->synth);

        // set music loop
        if (impl_->player == nullptr ||
            fluid_player_add(impl_->player, path.string().c_str()) != FLUID_OK) {
            std::fprintf(stderr, "could not load music %s\n", path.string().c_str());
            impl_->delete_player();
            impl_->music = Music::none;
        } else {
            (void)fluid_player_set_loop(impl_->player, music == Music::end ? 1 : -1);
            (void)fluid_player_play(impl_->player);
        }
    }
    SDL_UnlockMutex(impl_->mutex);
}

void Audio::stop_music() {
    play_music(Music::none);
}

Music Audio::current_music() const {
    return impl_->music;
}

void Audio::set_master_volume(float volume) {
    if (impl_->mutex == nullptr) {
        impl_->master_volume = std::clamp(volume, 0.0F, 1.0F);
        return;
    }
    SDL_LockMutex(impl_->mutex);
    impl_->master_volume = std::clamp(volume, 0.0F, 1.0F);
    SDL_UnlockMutex(impl_->mutex);
}

void Audio::set_effect_volume(float volume) {
    if (impl_->mutex != nullptr) {
        SDL_LockMutex(impl_->mutex);
    }
    impl_->effect_volume = std::clamp(volume, 0.0F, 1.0F);
    if (impl_->mutex != nullptr) {
        SDL_UnlockMutex(impl_->mutex);
    }
}

void Audio::set_music_volume(float volume) {
    if (impl_->mutex != nullptr) {
        SDL_LockMutex(impl_->mutex);
    }
    impl_->music_volume = std::clamp(volume, 0.0F, 1.0F);
    if (impl_->mutex != nullptr) {
        SDL_UnlockMutex(impl_->mutex);
    }
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
