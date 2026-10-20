#!/bin/sh
# The same SDK base and audio fixes used by the hardware-tested Slime Volley port.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sdk="$root/build/nxdk"
if [ -e "$sdk" ]; then
    printf 'SDK already exists: %s\n' "$sdk"
    exit 0
fi
mkdir -p "$root/build"
git clone https://github.com/XboxDev/nxdk.git "$sdk"
git -C "$sdk" checkout 14d5ee97e73347c973f1f57b68b79ec08c9e77f2
git -C "$sdk" apply "$root/xbox/patches/ac97-ready-link.patch"
git -C "$sdk" submodule update --init --recursive
git -C "$sdk" apply "$root/xbox/patches/cxbe-source-date-epoch.patch"
git -C "$sdk/lib/sdl/SDL2" apply "$root/xbox/patches/sdl-audio-shutdown.patch"
git -C "$sdk/lib/sdl/SDL2" apply "$root/xbox/patches/sdl-audio-buffer-headroom.patch"
