#pragma once

#include <memory>

struct State;

enum class Music {
    none,
    end,
    jamin,
    organjam,
    rock1,
    rock2,
    run,
    sweet,
};

enum class Sound {
    alien_hit,
    applause,
    attic_door,
    bounce,
    broken,
    chicken,
    chicken2,
    clink,
    clunk,
    down,
    dribble,
    drink,
    error,
    excellent,
    explode,
    harp,
    horn,
    intro,
    move,
    open_door,
    pickup,
    rev,
    warp,
    whistle,
    xgames,
    yeah,
};

class Audio {
  public:
    Audio();
    ~Audio();

    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    bool open();
    void play(Sound sound);
    bool is_playing(Sound sound) const;
    void play_music(Music music);
    void stop_music();
    Music current_music() const;
    void set_master_volume(float volume);
    void set_effect_volume(float volume);
    void set_music_volume(float volume);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

Music music_for_state(const State& state);
void sync_music(Audio& audio, const State& state);
