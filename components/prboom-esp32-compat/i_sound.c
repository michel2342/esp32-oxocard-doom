/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *  System interface for sound.
 *
 *-----------------------------------------------------------------------------
 */

#include "config.h"
#include <math.h>
#include <unistd.h>

#include "sdkconfig.h"
#include "z_zone.h"

#include "m_swap.h"
#include "i_sound.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"
#include "lprintf.h"
#include "s_sound.h"

#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"

#include "d_main.h"

int snd_card = 0;
int mus_card = 0;
int snd_samplerate = 0;

// ---------------------------------------------------------------------------
// Oxocard buzzer implementation (LEDC PWM tone generation)
// ---------------------------------------------------------------------------
#ifdef CONFIG_HW_OXOCARD

#include "driver/ledc.h"
#include "esp_timer.h"

typedef struct {
    uint32_t freq_hz;
    uint32_t duration_ms;
} buzzer_tone_t;

// Tone table indexed by sfxenum_t.  Entries left at {0,0} are silent.
// Gamma values and exact frequencies are tunable; these are a reasonable
// first pass for a piezo buzzer.
static const buzzer_tone_t tone_table[NUMSFX] = {
    [sfx_pistol]  = { 800,  80 },
    [sfx_shotgn]  = { 300, 120 },
    [sfx_dshtgn]  = { 250, 140 },
    [sfx_chgun]   = { 700,  60 },
    [sfx_rlaunc]  = { 450, 150 },
    [sfx_plasma]  = { 600, 100 },
    [sfx_bfg]     = { 200, 250 },
    [sfx_sawful]  = { 350,  40 },
    [sfx_sawhit]  = { 500,  60 },
    [sfx_punch]   = { 200,  80 },
    [sfx_itemup]  = {1200, 100 },
    [sfx_wpnup]   = {1000,  80 },
    [sfx_getpow]  = {1500, 150 },
    [sfx_doropn]  = { 220, 300 },
    [sfx_dorcls]  = { 180, 200 },
    [sfx_plpain]  = { 150, 100 },
    [sfx_swtchn]  = { 500,  40 },
    [sfx_swtchx]  = { 400,  40 },
    [sfx_telept]  = { 900, 120 },
    [sfx_barexp]  = { 180, 200 },
};

static esp_timer_handle_t tone_stop_timer = NULL;
static volatile int       tone_active    = 0;

static void tone_stop_cb(void *arg)
{
    (void)arg;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    tone_active = 0;
}

void I_InitSound(void)
{
    ledc_timer_config_t timer_conf = {
        .duty_cycle_res = LEDC_DUTY_RES_10_BIT,
        .freq_hz        = 1000,
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .timer_num      = LEDC_TIMER_0
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t ch_conf = {
        .channel    = LEDC_CHANNEL_0,
        .duty       = 0,
        .gpio_num   = CONFIG_HW_OXOCARD_BUZZER_GPIO,
        .intr_type  = LEDC_INTR_DISABLE,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num  = LEDC_TIMER_0
    };
    ledc_channel_config(&ch_conf);

    const esp_timer_create_args_t timer_args = {
        .callback = tone_stop_cb,
        .name     = "tone_stop"
    };
    esp_timer_create(&timer_args, &tone_stop_timer);

    lprintf(LO_INFO, "I_InitSound: buzzer on GPIO %d\n", CONFIG_HW_OXOCARD_BUZZER_GPIO);
}

// I_GetSfxLumpNum is called once per sfx during S_Init() to populate
// sfx->lumpnum.  We return the sfxenum_t index so I_StartSound can
// use it to index tone_table[].
int I_GetSfxLumpNum(sfxinfo_t* sfx)
{
    extern sfxinfo_t S_sfx[];
    return (int)(sfx - S_sfx);
}

int I_StartSound(int id, int channel, int vol, int sep, int pitch, int priority)
{
    if (id < 0 || id >= NUMSFX) return channel;

    const buzzer_tone_t *tone = &tone_table[id];
    if (tone->freq_hz == 0) return channel;   // no tone mapped

    // Preempt any currently playing tone
    if (tone_active) {
        esp_timer_stop(tone_stop_timer);
        tone_active = 0;
    }

    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, tone->freq_hz);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);   // 50 % of 1024 (10-bit)
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    tone_active = 1;

    // Auto-stop after duration (esp_timer takes microseconds)
    esp_timer_start_one_shot(tone_stop_timer, (uint64_t)tone->duration_ms * 1000ULL);

    return channel;
}

void I_StopSound(int handle)
{
    if (tone_active) {
        esp_timer_stop(tone_stop_timer);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        tone_active = 0;
    }
}

int I_SoundIsPlaying(int handle)
{
    return tone_active;
}

int I_AnySoundStillPlaying(void)
{
    return tone_active;
}

void I_ShutdownSound(void)
{
    I_StopSound(0);
    if (tone_stop_timer) {
        esp_timer_delete(tone_stop_timer);
        tone_stop_timer = NULL;
    }
}

void I_UpdateSoundParams(int handle, int volume, int seperation, int pitch)
{
}

void I_SetChannels(void)
{
}

// ---------------------------------------------------------------------------
// Non-Oxocard: original stubs (no audio hardware)
// ---------------------------------------------------------------------------
#else  /* ! CONFIG_HW_OXOCARD */

void I_UpdateSoundParams(int handle, int volume, int seperation, int pitch)
{
}

void I_SetChannels(void)
{
}

int I_GetSfxLumpNum(sfxinfo_t* sfx)
{
    return 1;
}

int I_StartSound(int id, int channel, int vol, int sep, int pitch, int priority)
{
  return channel;
}

void I_StopSound (int handle)
{
}

int I_SoundIsPlaying(int handle)
{
    return 0;
}

int I_AnySoundStillPlaying(void)
{
  return false;
}

void I_ShutdownSound(void)
{
}

void I_InitSound(void)
{
}

#endif  /* CONFIG_HW_OXOCARD */

// ---------------------------------------------------------------------------
// Music stubs — a single buzzer cannot play music; these are shared by both paths
// ---------------------------------------------------------------------------

void I_ShutdownMusic(void)
{
}

void I_InitMusic(void)
{
}

void I_PlaySong(int handle, int looping)
{
}

extern int mus_pause_opt; // From m_misc.c

void I_PauseSong (int handle)
{
}

void I_ResumeSong (int handle)
{
}

void I_StopSong(int handle)
{
}

void I_UnRegisterSong(int handle)
{
}

int I_RegisterSong(const void *data, size_t len)
{
  return (0);
}

int I_RegisterMusic( const char* filename, musicinfo_t *song )
{
    return 1;
}

void I_SetMusicVolume(int volume)
{
}


