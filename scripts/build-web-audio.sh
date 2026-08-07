#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
audio_root="$repo_root/site/audio"
soundfont="${AWC_SOUNDFONT:-/usr/share/sounds/sf2/TimGM6mb.sf2}"

if [[ ! -f "$soundfont" ]]; then
    printf 'General MIDI SoundFont not found: %s\n' "$soundfont" >&2
    exit 1
fi

rm -rf "$audio_root"
mkdir -p "$audio_root/sounds" "$audio_root/music"
cp "$repo_root"/assets/sounds/*.wav "$audio_root/sounds/"
ffmpeg -y -loglevel error -i "$repo_root/assets/sounds/Dribble.wav" -af volume=12dB \
    "$audio_root/sounds/Dribble.wav"

temporary_root="$(mktemp -d)"
trap 'rm -rf "$temporary_root"' EXIT
for midi in "$repo_root"/assets/music/*.mid; do
    name="$(basename "$midi" .mid)"
    wav="$temporary_root/$name.wav"
    fluidsynth -ni -r 44100 -F "$wav" "$soundfont" "$midi" >/dev/null
    ffmpeg -hide_banner -loglevel error -y -i "$wav" \
        -c:a libvorbis -q:a 5 "$audio_root/music/$name.ogg"
done
