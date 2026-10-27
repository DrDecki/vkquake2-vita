/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2017 Felipe Izzo
Copyright (C) 2026 DrDecki

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// snd_vita.c -- the mixer paints into a ring buffer, a thread hands it to
// the audio port one chunk at a time. The play position is the chunk the
// thread is on, so it can not drift away from what is really playing.

#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>

#include "../client/client.h"
#include "../client/snd_loc.h"

#define SAMPLE_RATE      48000
#define CHUNK_FRAMES     512                    // about 10.7 ms
#define CHUNKS           32                     // 16384 frames, more than the mix ahead
#define BUFFER_FRAMES    (CHUNK_FRAMES * CHUNKS)
#define BUFFER_BYTES     (BUFFER_FRAMES * 4)    // 16 bit stereo

static byte *audio_buffer;
static volatile int play_chunk;
static volatile int stop_audio;
static volatile int running;
static SceUID audio_thread_id = -1;

static int audio_thread(SceSize args, void *argp)
{
	int port;
	int vol[2] = { SCE_AUDIO_VOLUME_0DB, SCE_AUDIO_VOLUME_0DB };
	(void)args; (void)argp;

	port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, CHUNK_FRAMES, SAMPLE_RATE, SCE_AUDIO_OUT_MODE_STEREO);
	if (port < 0)
	{
		running = 0;
		return 0;
	}
	sceAudioOutSetVolume(port, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, vol);

	running = 1;
	while (!stop_audio)
	{
		// blocks until the previous chunk has been taken
		sceAudioOutOutput(port, audio_buffer + play_chunk * CHUNK_FRAMES * 4);
		play_chunk = (play_chunk + 1) % CHUNKS;
	}
	sceAudioOutOutput(port, NULL);
	sceAudioOutReleasePort(port);
	running = 0;
	return 0;
}

qboolean SNDDMA_Init(void)
{
	int res;

	Cvar_SetValue("s_khz", 48);
	Cvar_SetValue("s_loadas8bit", 0);
	if (s_mixahead && s_mixahead->value > 0.15f)
		Cvar_SetValue("s_mixahead", 0.1f);

	audio_buffer = calloc(1, BUFFER_BYTES);
	if (!audio_buffer)
		return false;

	dma.samplebits = 16;
	dma.speed = SAMPLE_RATE;
	dma.channels = 2;
	dma.samples = BUFFER_FRAMES * 2; // mono samples in the whole buffer
	dma.samplepos = 0;
	dma.submission_chunk = 1;
	dma.buffer = audio_buffer;

	play_chunk = 0;
	stop_audio = 0;
	audio_thread_id = sceKernelCreateThread("vkq2_sound", audio_thread, 0x10000100, 0x10000, 0, 0, NULL);
	if (audio_thread_id < 0)
	{
		Com_Printf("SNDDMA_Init: could not create the sound thread (0x%08X)\n", audio_thread_id);
		free(audio_buffer);
		audio_buffer = NULL;
		return false;
	}
	res = sceKernelStartThread(audio_thread_id, 0, NULL);
	if (res < 0)
	{
		Com_Printf("SNDDMA_Init: could not start the sound thread (0x%08X)\n", res);
		sceKernelDeleteThread(audio_thread_id);
		free(audio_buffer);
		audio_buffer = NULL;
		return false;
	}
	return true;
}

int SNDDMA_GetDMAPos(void)
{
	if (!audio_buffer)
		return 0;
	dma.samplepos = play_chunk * CHUNK_FRAMES * 2;
	return dma.samplepos;
}

void SNDDMA_Shutdown(void)
{
	if (!audio_buffer)
		return;
	stop_audio = 1;
	if (audio_thread_id >= 0)
	{
		sceKernelWaitThreadEnd(audio_thread_id, NULL, NULL);
		sceKernelDeleteThread(audio_thread_id);
	}
	audio_thread_id = -1;
	free(audio_buffer);
	audio_buffer = NULL;
}

void SNDDMA_BeginPainting(void)
{
}

void SNDDMA_Submit(void)
{
}
