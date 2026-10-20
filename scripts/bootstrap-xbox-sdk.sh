#!/bin/sh
# Pinned NXDK base, retained Slime Volley fixes, and Neverball DMA fixes.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sdk="$root/build/nxdk"
apply_sdk_patch()
{
    checkout=$1
    patch="$root/xbox/patches/$2"
    if git -C "$checkout" apply --reverse --check "$patch" 2>/dev/null; then
        return
    fi
    git -C "$checkout" apply --check "$patch"
    git -C "$checkout" apply "$patch"
}
if [ ! -e "$sdk" ]; then
    mkdir -p "$root/build"
    git clone https://github.com/XboxDev/nxdk.git "$sdk"
    git -C "$sdk" checkout 14d5ee97e73347c973f1f57b68b79ec08c9e77f2
    git -C "$sdk" submodule update --init --recursive
fi
apply_sdk_patch "$sdk" ac97-ready-link.patch
apply_sdk_patch "$sdk" ac97-dma-storage.patch
apply_sdk_patch "$sdk" cxbe-source-date-epoch.patch
apply_sdk_patch "$sdk/lib/sdl/SDL2" sdl-audio-shutdown.patch
apply_sdk_patch "$sdk/lib/sdl/SDL2" sdl-audio-buffer-headroom.patch
apply_sdk_patch "$sdk/lib/sdl/SDL2" sdl-audio-deinit.patch
