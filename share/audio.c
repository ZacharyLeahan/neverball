/*
 * Copyright (C) 2003 Robert Kooima
 *
 * NEVERBALL is  free software; you can redistribute  it and/or modify
 * it under the  terms of the GNU General  Public License as published
 * by the Free  Software Foundation; either version 2  of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT  ANY  WARRANTY;  without   even  the  implied  warranty  of
 * MERCHANTABILITY or  FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
 * General Public License for more details.
 */

#include <SDL.h>

#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "config.h"
#include "audio.h"
#include "common.h"
#include "fs.h"
#include "fs_ov.h"
#include "log.h"

/* Xbox audio safeguards adapted from neverball-nxdk ed3fd13f, including
 * 3c59f350 and aead2645; modern volume and lifecycle fixes retained. */

/*---------------------------------------------------------------------------*/

#ifdef NXDK
#define AUDIO_RATE 48000
#else
#define AUDIO_RATE 44100
#endif
#define AUDIO_CHAN 2

struct voice
{
    OggVorbis_File  vf;
    float          amp;
    float         damp;
    int           chan;
    int           play;
    int           loop;
    char         *name;
    struct voice *next;
};

static int   audio_state = 0;
static float sound_vol   = 1.0f;
static float music_vol   = 1.0f;

static SDL_AudioSpec spec;

static struct voice *music  = NULL;
static struct voice *queue  = NULL;
static struct voice *voices = NULL;
static short        *buffer = NULL;

#ifdef NXDK
static unsigned int audio_callbacks, audio_eof, audio_decode_errors, audio_seek_errors;
#endif

#ifndef NXDK
static ov_callbacks callbacks = {
    fs_ov_read, fs_ov_seek, fs_ov_close, fs_ov_tell
};
#else
/* Keep Xbox filesystem operations off the high-priority audio thread. */
struct memory_audio
{
    unsigned char *bytes;
    long size, position;
};

static size_t memory_audio_read(void *ptr, size_t size, size_t count, void *data)
{
    struct memory_audio *source = data;
    size_t available;
    if (!size) return 0;
    available = (size_t)(source->size - source->position) / size;
    if (count > available) count = available;
    memcpy(ptr, source->bytes + source->position, size * count);
    source->position += (long)(size * count);
    return count;
}

static int memory_audio_seek(void *data, ogg_int64_t offset, int whence)
{
    struct memory_audio *source = data;
    ogg_int64_t base;
    switch (whence)
    {
    case SEEK_SET: base = 0; break;
    case SEEK_CUR: base = source->position; break;
    case SEEK_END: base = source->size; break;
    default: return -1;
    }
    if (offset < -base || offset > source->size - base) return -1;
    source->position = (long)(base + offset);
    return 0;
}

static long memory_audio_tell(void *data)
{
    return ((struct memory_audio *)data)->position;
}

static int memory_audio_close(void *data)
{
    struct memory_audio *source = data;
    free(source->bytes);
    free(source);
    return 0;
}

static ov_callbacks callbacks = {
    memory_audio_read, memory_audio_seek, memory_audio_close, memory_audio_tell
};

static struct memory_audio *memory_audio_load(fs_file file)
{
    struct memory_audio *source = calloc(1, sizeof(*source));
    if (!source) return NULL;
    if (fs_seek(file, 0, SEEK_END) || (source->size = fs_tell(file)) <= 0 ||
        fs_seek(file, 0, SEEK_SET) || !(source->bytes = malloc(source->size)) ||
        fs_read(source->bytes, source->size, file) != (size_t)source->size)
    {
        memory_audio_close(source);
        return NULL;
    }
    return source;
}
#endif

/*---------------------------------------------------------------------------*/

#define LOG_VOLUME(v) ((float) pow((double) (v), 2.0))

#define MIX(d, s) {                           \
        int T = (int) (d) + (int) (s);        \
        if      (T >  32767) (d) =  32767;    \
        else if (T < -32768) (d) = -32768;    \
        else                 (d) = (short) T; \
    }

static int voice_step(struct voice *V, float volume, Uint8 *stream, int length)
{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    int order = 1;
#else
    int order = 0;
#endif

    short *obuf = (short *) stream;
    char  *ibuf = (char  *) buffer;

    int i, b = 0, n = 1, c = 0, r = 0;
    int retries = 0;

    /* Compute the total request size for the current stream. */

    if (V->chan == 1) r = length / 2;
    if (V->chan == 2) r = length    ;

    /* While data is coming in and data is still needed... */

    while (n > 0 && r > 0)
    {
        /* Read audio from the stream. */

        if ((n = (int) ov_read(&V->vf, ibuf, r, order, 2, 1, &b)) > 0)
        {
            retries = 0;
            /* Mix mono audio. */

            if (V->chan == 1)
                for (i = 0; i < n / 2; i += 1)
                {
                    short M = (short) (LOG_VOLUME(V->amp) * volume * buffer[i]);

                    MIX(obuf[c], M); c++;
                    MIX(obuf[c], M); c++;

                    V->amp += V->damp;

                    if (V->amp < 0.0f) V->amp = 0.0;
                    if (V->amp > 1.0f) V->amp = 1.0;
                }

            /* Mix stereo audio. */

            if (V->chan == 2)
                for (i = 0; i < n / 2; i += 2)
                {
                    short L = (short) (LOG_VOLUME(V->amp) * volume * buffer[i + 0]);
                    short R = (short) (LOG_VOLUME(V->amp) * volume * buffer[i + 1]);

                    MIX(obuf[c], L); c++;
                    MIX(obuf[c], R); c++;

                    V->amp += V->damp;

                    if (V->amp < 0.0f) V->amp = 0.0;
                    if (V->amp > 1.0f) V->amp = 1.0;
                }

            r -= n;
        }
        else
        {
#ifdef NXDK
            if (n == 0) audio_eof++;
            else audio_decode_errors++;
#endif
            /* A callback must return even if a looping stream is empty,
             * damaged, or cannot seek. Otherwise the high-priority Xbox
             * audio thread can starve gameplay and the recovery watchdog. */
            if (++retries > 4)
                return 1;
            if (n == OV_HOLE)
                n = 1;
            else if (n == 0 && V->loop)
            {
                if (ov_pcm_seek(&V->vf, 0) != 0)
                {
#ifdef NXDK
                    audio_seek_errors++;
#endif
                    return 1;
                }
                n = 1;
            }
            else return 1;
        }
    }
    return 0;
}

static struct voice *voice_init(const char *filename, float a)
{
    struct voice *V;
    fs_file      fp;

    if (!filename)
        return NULL;

    /* Allocate and initialize a new voice structure. */

    if ((V = (struct voice *) calloc(1, sizeof (struct voice))))
    {
        if (!(V->name = strdup(filename)))
        {
            free(V);
            return NULL;
        }

        /* Attempt to open the named Ogg stream. */

        if ((fp = fs_open_read(filename)))
        {
            int opened;
#ifdef NXDK
            struct memory_audio *source = memory_audio_load(fp);
            fs_close(fp);
            opened = source ? ov_open_callbacks(source, &V->vf, NULL, 0, callbacks) : -1;
            if (opened != 0 && source) memory_audio_close(source);
#else
            opened = ov_open_callbacks(fp, &V->vf, NULL, 0, callbacks);
            if (opened != 0) fs_close(fp);
#endif
            if (opened == 0)
            {
                vorbis_info *info = ov_info(&V->vf, -1);

                /* On success, configure the voice. */

                V->amp  = a;
                V->damp = 0;
                V->chan = info->channels;
                V->play = 1;
                V->loop = 0;

                if (V->amp > 1.0f) V->amp = 1.0;
                if (V->amp < 0.0f) V->amp = 0.0;

                /* Clearing the Ogg releases its file or cached source. */
                return V;
            }
        }
        free(V->name);
        free(V);
    }
    return NULL;
}

static void voice_free(struct voice *V)
{
    if (V)
    {
        ov_clear(&V->vf);

        free(V->name);
        free(V);
    }
}

/*---------------------------------------------------------------------------*/

static void audio_step(void *data, Uint8 *stream, int length)
{
    struct voice *V = voices;
    struct voice *P = NULL;
#ifdef NXDK
    audio_callbacks++;
#endif

    /* Zero the output buffer. */

    memset(stream, 0, length);

    /* Mix the background music. */

    if (music)
    {
        int ended = !music->play || voice_step(music, music_vol, stream, length);
        if (ended)
            music->play = 0;

        /* If the track has faded out, move to a queued track. */

        if (queue && (ended || (music->amp <= 0.0f && music->damp < 0.0f)))
        {
            voice_free(music);
            music = queue;
            queue = NULL;
        }
    }

    /* Iterate over all active voices. */

    while (V)
    {
        /* Mix this voice. */

        if (V->play && voice_step(V, sound_vol, stream, length))
        {
            /* Delete a finished voice... */

            struct voice *T = V;

            if (P)
                V = P->next = V->next;
            else
                V = voices  = V->next;

            voice_free(T);
        }
        else
        {
            /* ... or continue to the next. */

            P = V;
            V = V->next;
        }
    }
}

/*---------------------------------------------------------------------------*/

void audio_init(void)
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
    {
        log_printf("Failure to initialize audio (%s)\n", SDL_GetError());
        return;
    }

    audio_state = 0;

#ifdef NXDK
    /* Hardware diagnostic switch: isolate graphics/input without a noisy
     * stalled audio buffer. Remove this file to test sound again. */
    if (fs_exists("no-audio"))
    {
        log_printf("Audio disabled by hardware diagnostic marker\n");
        return;
    }
#endif

    /* Configure the audio. */

    spec.format   = AUDIO_S16SYS;
    spec.channels = AUDIO_CHAN;
#ifdef NXDK
    spec.samples  = 1024; /* Match the native Xbox driver; avoid live conversion. */
#else
    spec.samples  = config_get_d(CONFIG_AUDIO_BUFF);
#endif
    spec.freq     = AUDIO_RATE;
    spec.callback = audio_step;

    /* Allocate an input buffer. */

    if ((buffer = (short *) malloc(spec.samples * 4)))
    {
        /* Start the audio thread. */

        if (SDL_OpenAudio(&spec, NULL) == 0)
        {
#ifdef NXDK
            printf("audio: driver=%s rate=%d channels=%d samples=%u bytes=%u\n",
                   SDL_GetCurrentAudioDriver(), spec.freq, spec.channels,
                   spec.samples, spec.size);
#endif
            audio_state = 1;
            SDL_PauseAudio(0);
        }
        else log_printf("Failure to open audio device (%s)\n", SDL_GetError());
    }

    /* Set the initial volumes. */

    audio_volume(config_get_d(CONFIG_SOUND_VOLUME),
                 config_get_d(CONFIG_MUSIC_VOLUME));
}

void audio_free(void)
{
    struct voice *V;

    /* Halt the audio thread. */

    SDL_CloseAudio();
#ifdef NXDK
    printf("audio: callbacks=%u eof=%u decode_errors=%u seek_errors=%u\n",
           audio_callbacks, audio_eof, audio_decode_errors, audio_seek_errors);
#endif

    /* Release the input buffer. */

    free(buffer);
    buffer = NULL;

    /* Free the voices. */

    voice_free(music);
    voice_free(queue);

    V = voices;
    audio_state = 0;

    while (V)
    {
        struct voice *N = V->next;
        voice_free(V);
        V = N;
    }

    voices = NULL;
    music = NULL;
    queue = NULL;

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void audio_play(const char *filename, float a)
{
    if (audio_state)
    {
        struct voice *V;

        /* If we're already playing this sound, preempt the running copy. */

        SDL_LockAudio();
        {
            for (V = voices; V; V = V->next)
                if (V->name && strcmp(V->name, filename) == 0)
                {
                    ov_raw_seek(&V->vf, 0);

                    V->amp = a;

                    if (V->amp > 1.0f) V->amp = 1.0;
                    if (V->amp < 0.0f) V->amp = 0.0;

                    SDL_UnlockAudio();
                    return;
                }
        }
        SDL_UnlockAudio();

        /* Create a new voice structure. */

        if ((V = voice_init(filename, a)))
        {
            /* Add it to the list of sounding voices. */

            SDL_LockAudio();
            {
                V->next = voices;
                voices  = V;
            }
            SDL_UnlockAudio();
        }
    }
}

/*---------------------------------------------------------------------------*/

static void audio_music_play(const char *filename)
{
    if (audio_state)
    {
        /* File reads and decoder setup must not block the mixer. */
        struct voice *next = voice_init(filename, 0.0f);
        struct voice *old;
        if (next) next->loop = 1;

        SDL_LockAudio();
        old = music;
        music = next;
        SDL_UnlockAudio();
        if (old) voice_free(old);
    }
}

static void audio_music_queue(const char *filename, float t)
{
    if (audio_state)
    {
        struct voice *next = voice_init(filename, 0.0f);
        struct voice *old;
        if (next)
        {
            next->loop = 1;
            if (t > 0.0f) next->damp = +1.0f / (AUDIO_RATE * t);
        }
        SDL_LockAudio();
        old = queue;
        queue = next;
        SDL_UnlockAudio();
        if (old) voice_free(old);
    }
}

void audio_music_stop(void)
{
    if (audio_state)
    {
        struct voice *old;
        SDL_LockAudio();
        old = music;
        music = NULL;
        SDL_UnlockAudio();
        if (old) voice_free(old);
    }
}

/*---------------------------------------------------------------------------*/

void audio_music_fade_out(float t)
{
    SDL_LockAudio();
    {
        if (music) music->damp = -1.0f / (AUDIO_RATE * t);
    }
    SDL_UnlockAudio();
}

void audio_music_fade_in(float t)
{
    SDL_LockAudio();
    {
        if (music) music->damp = +1.0f / (AUDIO_RATE * t);
    }
    SDL_UnlockAudio();
}

void audio_music_fade_to(float t, const char *filename)
{
    struct voice *discard = NULL;
    int playing, same;
    if (!audio_state) return;

    SDL_LockAudio();
    playing = music != NULL;
    same = playing && strcmp(filename, music->name) == 0;
    if (same)
    {
        discard = queue;
        queue = NULL;
        music->damp = t > 0.0f ? +1.0f / (AUDIO_RATE * t) : 0.0f;
        if (t <= 0.0f) music->amp = 1.0f;
    }
    SDL_UnlockAudio();
    if (discard) voice_free(discard);

    if (same) return;
    if (playing)
    {
        audio_music_fade_out(t);
        audio_music_queue(filename, t);
    }
    else
    {
        audio_music_play(filename);
        audio_music_fade_in(t);
    }
}

/*
 * Logarithmic volume control.
 */
void audio_volume(int s, int m)
{
    float sl = (float) s / 10.0f;
    float ml = (float) m / 10.0f;

    sound_vol = LOG_VOLUME(sl);
    music_vol = LOG_VOLUME(ml);
}

/*---------------------------------------------------------------------------*/
