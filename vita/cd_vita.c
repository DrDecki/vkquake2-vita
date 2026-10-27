/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2026 DrDecki

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

*/
// cd_vita.c -- there is no CD drive. Music from baseq2/music/*.ogg goes
// through the Miniaudio_ functions, which are not hooked up yet.

#include "../client/client.h"

int CDAudio_Init(void)
{
	return 0;
}

void CDAudio_Shutdown(void)
{
}

qboolean CDAudio_Play(int track, qboolean looping)
{
	(void)track; (void)looping;
	return false;
}

void CDAudio_Stop(void)
{
}

void CDAudio_Update(void)
{
}

void CDAudio_Activate(qboolean active)
{
	(void)active;
}

void Miniaudio_Init(void)
{
}

void Miniaudio_Play(int track, qboolean looping)
{
	(void)track; (void)looping;
}

void Miniaudio_Stop(void)
{
}

void Miniaudio_Update(void)
{
}

void Miniaudio_Shutdown(void)
{
}
