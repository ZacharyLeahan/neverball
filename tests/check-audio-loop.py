#!/usr/bin/env python3
"""Compile the real mixer function against bounded decoder fault scenarios."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = (root / 'share/audio.c').read_text()
voice = source[source.index('struct voice\n'):source.index('static int   audio_state')]
mix = source[source.index('#define MIX('):source.index('static int voice_step')]
step = source[source.index('static int voice_step'):source.index('static struct voice *voice_init')]
prelude = r'''
#include <math.h>
#define LOG_VOLUME(v) ((float)pow((double)(v), 2.0))
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef int OggVorbis_File;
typedef unsigned char Uint8;
#define SDL_BIG_ENDIAN 4321
#define SDL_BYTEORDER 1234
#define OV_HOLE -3
static short storage[64];
static short *buffer = storage;
static int mode, calls, seeks;
static long ov_read(OggVorbis_File *vf, char *out, int size, int order, int width, int sign, int *section) {
    (void)vf;(void)size;(void)order;(void)width;(void)sign;(void)section;
    assert(++calls < 16); /* Old unbounded callback fails instead of hanging this check. */
    if(mode==0 || mode==1)return 0;
    if(mode==2)return -137; /* terminal decode error */
    if(mode==3)return OV_HOLE;
    if((mode==4 || mode==5) && calls==1)return mode==4 ? 0 : OV_HOLE;
    short pcm[4]={100,200,300,400};memcpy(out,pcm,sizeof(pcm));return sizeof(pcm);
}
static int ov_pcm_seek(OggVorbis_File *vf, long long offset) {
    (void)vf;(void)offset;++seeks;return mode==1 ? -1 : 0;
}
'''
tests = r'''
int main(void) {
    for(mode=0;mode<=5;++mode) {
        struct voice v={0};short out[4]={0};v.chan=2;v.amp=1;v.loop=1;
        calls=seeks=0;
        int ended=voice_step(&v,1,(Uint8*)out,sizeof(out));
        if(mode<4)assert(ended==1);else {assert(ended==0);assert(out[0]==100 && out[3]==400);}
        assert(calls<=5);if(mode==1)assert(seeks==1);if(mode==2 || mode==3)assert(seeks==0);
    }
    puts("Mixer checks passed: empty loops, failed seeks, decode errors, holes, and successful restarts");
}
'''
with tempfile.TemporaryDirectory(prefix='neverball-mixer-') as directory:
    path=Path(directory)/'check.c';binary=Path(directory)/'check'
    path.write_text(prelude+voice+mix+step+tests)
    subprocess.run(['cc','-fsanitize=address,undefined','-g',str(path),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
