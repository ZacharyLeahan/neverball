#!/usr/bin/env python3
"""Decode real music through the Xbox RAM callbacks and mixer on the host."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'share/audio.c').read_text()
voice=s[s.index('struct voice\n'):s.index('static int   audio_state')]
callbacks=s[s.index('struct memory_audio\n'):s.index('static struct memory_audio *memory_audio_load')]
mix=s[s.index('#define MIX('):s.index('static int voice_step')]
step=s[s.index('static int voice_step'):s.index('static struct voice *voice_init')]
prelude=r'''
#include <math.h>
#define LOG_VOLUME(v) ((float)pow((double)(v), 2.0))
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vorbis/vorbisfile.h>
#define NXDK 1
#define SDL_BIG_ENDIAN 4321
#define SDL_BYTEORDER 1234
typedef unsigned char Uint8;
static short storage[2048];static short *buffer=storage;
static unsigned audio_eof,audio_decode_errors,audio_seek_errors;
'''
body=r'''
int main(int argc,char **argv) {
    assert(argc==2);
    FILE *f=fopen(argv[1],"rb");assert(f);assert(!fseek(f,0,SEEK_END));long size=ftell(f);rewind(f);
    struct memory_audio *src=calloc(1,sizeof(*src));assert(src);src->size=size;src->bytes=malloc(size);assert(src->bytes);
    assert(fread(src->bytes,1,size,f)==(size_t)size);fclose(f);
    assert(memory_audio_seek(src,-1,SEEK_SET)==-1);
    assert(memory_audio_seek(src,1,SEEK_END)==-1);
    assert(memory_audio_seek(src,0,SEEK_SET)==0);
    struct voice v={0};assert(!ov_open_callbacks(src,&v.vf,NULL,0,callbacks));
    v.chan=ov_info(&v.vf,-1)->channels;assert(v.chan==1 || v.chan==2);v.amp=1;v.loop=1;
    short out[2048];unsigned active=0;
    for(int i=0;i<8000;++i) {
        memset(out,0,sizeof(out));assert(!voice_step(&v,1,(Uint8*)out,sizeof(out)));
        for(unsigned j=0;j<sizeof(out)/sizeof(out[0]);++j)active+=out[j]!=0;
    }
    assert(active>1000000);assert(audio_eof>=2);assert(!audio_decode_errors && !audio_seek_errors);
    printf("RAM music checks passed: %u successful loops, %u active samples, no decode/seek errors\n",audio_eof,active);
    ov_clear(&v.vf);
}
'''
with tempfile.TemporaryDirectory(prefix='neverball-audio-memory-') as d:
    path=Path(d)/'check.c';exe=Path(d)/'check';path.write_text(prelude+voice+callbacks+mix+step+body)
    subprocess.run(['cc','-fsanitize=address,undefined','-g','-I/opt/homebrew/include',str(path),'-L/opt/homebrew/lib','-lvorbisfile','-lvorbis','-logg','-o',str(exe)],check=True)
    for asset in [root/'data/bgm/title.ogg', *sorted((root/'data/snd').glob('*.ogg'))[:3]]:
        print(asset.relative_to(root), flush=True)
        subprocess.run([str(exe),str(asset)],check=True)
