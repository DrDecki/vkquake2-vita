/*
Copyright (C) 2018-2019 Krzysztof Kondrak
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

#include <psp2/audioout.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>

#include "../client/client.h"
#include "vita.h"

#define STB_VORBIS_NO_PUSHDATA_API
#include "../client/miniaudio/stb_vorbis.c"

#define MUSIC_FRAMES	2048
#define MUSIC_PRIORITY	0xB0

static cvar_t *cd_volume;
static cvar_t *cd_loopcount;
static cvar_t *cd_looptrack;
static cvar_t *no_music;

static volatile int req_seq;
static char req_path[MAX_OSPATH];
static volatile int req_restart;
static volatile int req_paused;
static volatile int req_volume;

static volatile int finished_seq = -1;
static volatile int failed_seq = -1;

static SceUID music_thread_id = -1;
static volatile int music_quit;

static qboolean enabled = true;
static int play_track;
static char play_path[MAX_OSPATH];
static qboolean play_looping;
static int loopcounter;

static short pcm[2][MUSIC_FRAMES * 2];

static unsigned char *load_file(const char *path, int *size)
{
	unsigned char *data;
	SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	int len, got = 0;

	if (fd < 0)
		return NULL;
	len = (int)sceIoLseek(fd, 0, SCE_SEEK_END);
	sceIoLseek(fd, 0, SCE_SEEK_SET);
	data = len > 0 ? malloc(len) : NULL;
	while (data && got < len)
	{
		int r = sceIoRead(fd, data + got, len - got);
		if (r <= 0)
			break;
		got += r;
	}
	sceIoClose(fd);
	if (data && got != len)
	{
		free(data);
		data = NULL;
	}
	*size = len;
	return data;
}

static int music_thread(SceSize args, void *argp)
{
	stb_vorbis *v = NULL;
	unsigned char *file = NULL;
	char path[MAX_OSPATH];
	int port = -1, rate = 0, seen_seq = -1, cur = 0, volume = -1, done = 0;

	while (!music_quit)
	{
		if (seen_seq != req_seq)
		{
			int seq = req_seq, restart;
			if (seq < 0)
			{
				sceKernelDelayThread(1000);
				continue;
			}
			__sync_synchronize();
			memcpy(path, req_path, sizeof(path));
			path[sizeof(path) - 1] = 0;
			restart = req_restart;
			__sync_synchronize();
			if (seq != req_seq)
				continue;
			seen_seq = seq;
			done = 0;

			if (restart && v)
				stb_vorbis_seek_start(v);
			else
			{
				if (v)
					stb_vorbis_close(v);
				v = NULL;
				free(file);
				file = NULL;
				if (path[0])
				{
					int err, size;
					file = load_file(path, &size);
					if (file)
						v = stb_vorbis_open_memory(file, size, &err, NULL);
					if (!v)
						failed_seq = seq;
				}
				if (v)
				{
					stb_vorbis_info info = stb_vorbis_get_info(v);
					if (port < 0 || (int)info.sample_rate != rate)
					{
						if (port >= 0)
							sceAudioOutReleasePort(port);
						rate = (int)info.sample_rate;
						port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, MUSIC_FRAMES, rate,
						                           SCE_AUDIO_OUT_MODE_STEREO);
						volume = -1;
						if (port < 0)
						{
							stb_vorbis_close(v);
							v = NULL;
							failed_seq = seq;
						}
					}
				}
			}
		}

		if (!v || done || req_paused)
		{
			sceKernelDelayThread(20000);
			continue;
		}
		if (volume != req_volume)
		{
			int vol[2];
			volume = req_volume;
			vol[0] = vol[1] = volume;
			sceAudioOutSetVolume(port, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, vol);
		}

		int got = stb_vorbis_get_samples_short_interleaved(v, 2, pcm[cur], MUSIC_FRAMES * 2);
		SceUInt64 t1 = sceKernelGetProcessTimeWide();
		if (got <= 0)
		{
			done = 1;
			finished_seq = seen_seq;
			continue;
		}
		if (got < MUSIC_FRAMES)
			memset(pcm[cur] + got * 2, 0, (MUSIC_FRAMES - got) * 2 * sizeof(short));
		sceAudioOutOutput(port, pcm[cur]);
		cur ^= 1;

		SceUInt64 t2 = sceKernelGetProcessTimeWide();

		if (t2 - t1 < 1000)
			sceKernelDelayThread((SceUInt)(1000000ULL * MUSIC_FRAMES / rate / 2));
	}

	if (v)
		stb_vorbis_close(v);
	free(file);
	if (port >= 0)
	{
		sceAudioOutOutput(port, NULL);
		sceAudioOutReleasePort(port);
	}
	return 0;
}

static void send_request(const char *path, int restart)
{
	int seq = req_seq + 1;
	req_seq = -2;
	__sync_synchronize();
	Com_sprintf(req_path, sizeof(req_path), "%s", path ? path : "");
	req_restart = restart;
	__sync_synchronize();
	req_seq = seq;
}

static int volume_for(float v)
{
	if (v < 0)
		v = 0;
	if (v > 1)
		v = 1;
	return (int)(v * SCE_AUDIO_VOLUME_0DB);
}

static qboolean file_exists(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return false;
	fclose(f);
	return true;
}

static qboolean find_track(int track, char *path, int size)
{
	Com_sprintf(path, size, "%s/music/track%02i.ogg", FS_Gamedir(), track);
	if (file_exists(path))
		return true;
	Com_sprintf(path, size, VITA_BASEDIR "/" BASEDIRNAME "/music/track%02i.ogg", track);
	return file_exists(path);
}

static void Miniaudio_Pause(void)
{
	req_paused = 1;
}

static void Miniaudio_Resume(void)
{
	req_paused = 0;
}

void Miniaudio_Stop(void)
{
	play_track = 0;
	if (music_thread_id >= 0)
		send_request(NULL, 0);
}

void Miniaudio_Play(int track, qboolean looping)
{
	char path[MAX_OSPATH];

	if (!enabled || music_thread_id < 0 || play_track == track)
		return;
	Miniaudio_Stop();
	if (track < 1)
		return;

	if (!find_track(track, path, sizeof(path)))
	{
		Com_Printf("Music: %s/music/track%02i.ogg not found\n", FS_Gamedir(), track);
		return;
	}

	loopcounter = 0;
	play_track = track;
	play_looping = looping;
	req_paused = cd_volume->value == 0;
	req_volume = volume_for(cd_volume->value);
	Com_sprintf(play_path, sizeof(play_path), "%s", path);
	send_request(play_path, 0);
}

static void Miniaudio_f(void)
{
	char *command;

	if (Cmd_Argc() < 2)
		return;
	command = Cmd_Argv(1);

	if (!Q_strcasecmp(command, "on"))
		enabled = true;
	else if (!Q_strcasecmp(command, "off"))
	{
		Miniaudio_Stop();
		enabled = false;
	}
	else if (!Q_strcasecmp(command, "play"))
		Miniaudio_Play(atoi(Cmd_Argv(2)), false);
	else if (!Q_strcasecmp(command, "loop"))
		Miniaudio_Play(atoi(Cmd_Argv(2)), true);
	else if (!Q_strcasecmp(command, "stop"))
		Miniaudio_Stop();
	else if (!Q_strcasecmp(command, "pause"))
		Miniaudio_Pause();
	else if (!Q_strcasecmp(command, "resume"))
		Miniaudio_Resume();
	else if (!Q_strcasecmp(command, "info"))
	{
		if (play_track)
			Com_Printf("%s %s track %i\n", req_paused ? "Paused" : "Playing", play_looping ? "looping" : "once",
			           play_track);
		else
			Com_Printf("No music is playing.\n");
	}
}

void Miniaudio_Init(void)
{
	cd_volume = Cvar_Get("cd_volume", "1", CVAR_ARCHIVE);
	cd_loopcount = Cvar_Get("cd_loopcount", "4", 0);
	cd_looptrack = Cvar_Get("cd_looptrack", "11", 0);
	no_music = Cvar_Get("no_music", "0", 0);
	enabled = no_music->value == 0;
	req_paused = 0;
	req_volume = volume_for(cd_volume->value);

	music_quit = 0;
	music_thread_id = sceKernelCreateThread("vkq2_music", music_thread, MUSIC_PRIORITY, 0x20000, 0, 0, NULL);
	if (music_thread_id < 0 || sceKernelStartThread(music_thread_id, 0, NULL) < 0)
	{
		Com_Printf("Music: could not start the music thread\n");
		if (music_thread_id >= 0)
			sceKernelDeleteThread(music_thread_id);
		music_thread_id = -1;
	}
	Cmd_AddCommand("miniaudio", Miniaudio_f);
}

void Miniaudio_Update(void)
{
	if (no_music->value != 0 || music_thread_id < 0)
		return;

	if (cd_volume->modified)
	{
		cd_volume->modified = false;
		if (cd_volume->value < 0.f)
			Cvar_SetValue("cd_volume", 0.f);
		if (cd_volume->value > 1.f)
			Cvar_SetValue("cd_volume", 1.f);
		req_volume = volume_for(cd_volume->value);
	}

	if ((cd_volume->value == 0) != !enabled)
	{
		if (cd_volume->value == 0)
		{
			Miniaudio_Pause();
			enabled = false;
		}
		else
		{
			int track = atoi(cl.configstrings[CS_CDTRACK]);
			enabled = true;
			if (play_track != track && track > 0)
				Miniaudio_Play(track, true);
			else
				Miniaudio_Resume();
		}
	}

	if (play_track && failed_seq == req_seq)
	{
		Com_Printf("Music: could not play track %02i\n", play_track);
		play_track = 0;
		return;
	}

	if (enabled && play_track && finished_seq == req_seq)
	{
		if (!play_looping)
		{
			play_track = 0;
			return;
		}

		if (++loopcounter == (int)cd_loopcount->value && (int)cd_looptrack->value != play_track)
		{
			int ambient = (int)cd_looptrack->value;
			play_track = 0;
			Miniaudio_Play(ambient, true);
			loopcounter = (int)cd_loopcount->value;
		}
		else
			send_request(play_path, 1);
	}
}

void Miniaudio_Shutdown(void)
{
	Miniaudio_Stop();
	if (music_thread_id >= 0)
	{
		music_quit = 1;
		sceKernelWaitThreadEnd(music_thread_id, NULL, NULL);
		sceKernelDeleteThread(music_thread_id);
		music_thread_id = -1;
	}
	Cmd_RemoveCommand("miniaudio");
}
