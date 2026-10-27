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
// sys_vita.c -- PS Vita system layer: entry point, hunk, files, time, log.
// The hunk and directory search follow vitaQuakeII by Felipe Izzo.

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/threadmgr/vfp.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/io/dirent.h>
#include <psp2/ctrl.h>
#include <psp2/power.h>

#include <vkvita/vkvita.h>

#include "../qcommon/qcommon.h"
#include "vita.h"

// Most memory goes to the newlib heap (hunks, zone, sound). VkVita takes its
// GPU memory from memory blocks of its own, so it is not part of this.
int _newlib_heap_size_user = VITA_HEAP_SIZE;
int sceUserMainThreadStackSize = 8 * 1024 * 1024;

unsigned sys_frame_time;
int curtime;

static FILE *log_file;

void *GetGameAPI(void *import);

/* ------------------------------------------------------------------------ */
/* Log                                                                      */
/* ------------------------------------------------------------------------ */
void Vita_Log(const char *fmt, ...)
{
	va_list ap;
	if (!log_file)
	{
		sceIoMkdir(VITA_BASEDIR, 0777);
		log_file = fopen(VITA_BASEDIR "/vkquake2.log", "w");
		if (!log_file)
			return;
	}
	va_start(ap, fmt);
	vfprintf(log_file, fmt, ap);
	va_end(ap);
	fflush(log_file);
}

void Sys_ConsoleOutput(char *string)
{
	Vita_Log("%s", string);
}

void Sys_Printf(char *fmt, ...)
{
	va_list ap;
	char text[2048];
	va_start(ap, fmt);
	vsnprintf(text, sizeof(text), fmt, ap);
	va_end(ap);
	Vita_Log("%s", text);
}

char *Sys_ConsoleInput(void)
{
	return NULL;
}

/* ------------------------------------------------------------------------ */
/* Errors and quitting                                                      */
/* ------------------------------------------------------------------------ */
void Sys_Quit(void)
{
	CL_Shutdown();
	Qcommon_Shutdown();
	if (log_file)
		fclose(log_file);
	sceKernelExitProcess(0);
}

void Sys_Error(char *error, ...)
{
	va_list ap;
	char text[1024];

	CL_Shutdown();
	va_start(ap, error);
	vsnprintf(text, sizeof(text), error, ap);
	va_end(ap);
	Vita_Log("Error: %s\n", text);
	if (log_file)
		fclose(log_file);
	log_file = NULL;
	sceKernelExitProcess(0);
	for (;;)
		;
}

void Sys_Warn(char *warning, ...)
{
	va_list ap;
	char text[1024];
	va_start(ap, warning);
	vsnprintf(text, sizeof(text), warning, ap);
	va_end(ap);
	Vita_Log("Warning: %s", text);
}

void Sys_Init(void)
{
}

void Sys_AppActivate(void)
{
}

void Sys_CopyProtect(void)
{
}

char *Sys_GetClipboardData(void)
{
	return NULL;
}

/* ------------------------------------------------------------------------ */
/* Game library, linked in                                                  */
/* ------------------------------------------------------------------------ */
void Sys_UnloadGame(void)
{
}

void *Sys_GetGameAPI(void *parms)
{
	return GetGameAPI(parms);
}

/* ------------------------------------------------------------------------ */
/* Hunk: plain malloc, the code keeps pointers into it so it never moves    */
/* ------------------------------------------------------------------------ */
static byte *membase;
static int hunkmaxsize;
static int cursize;

void *Hunk_Begin(int maxsize)
{
	hunkmaxsize = maxsize;
	cursize = 0;
	membase = malloc(hunkmaxsize);
	if (!membase)
		Sys_Error("unable to allocate %d bytes for the hunk", hunkmaxsize);
	memset(membase, 0, hunkmaxsize);
	return membase;
}

void *Hunk_Alloc(int size)
{
	byte *buf;
	size = (size + 31) & ~31;
	if (cursize + size > hunkmaxsize)
		Sys_Error("Hunk_Alloc overflow");
	buf = membase + cursize;
	cursize += size;
	return buf;
}

int Hunk_End(void)
{
	return cursize;
}

void Hunk_Free(void *base)
{
	if (base == membase)
		membase = NULL;
	free(base);
}

/* ------------------------------------------------------------------------ */
/* Time and files                                                           */
/* ------------------------------------------------------------------------ */
int Sys_Milliseconds(void)
{
	static uint64_t base;
	uint64_t now = sceKernelGetProcessTimeWide() / 1000;
	if (!base)
		base = now;
	curtime = (int)(now - base);
	return curtime;
}

void Sys_Mkdir(char *path)
{
	sceIoMkdir(path, 0777);
}

int Sys_FileTime(char *path)
{
	SceIoStat st;
	if (sceIoGetstat(path, &st) < 0)
		return -1;
	return 1;
}

char *strlwr(char *s)
{
	char *p = s;
	while (*p)
	{
		*p = tolower((unsigned char)*p);
		p++;
	}
	return s;
}

int glob_match(char *pattern, char *text);

static void copy_string(char *dst, const char *src, size_t size)
{
	strncpy(dst, src, size - 1);
	dst[size - 1] = 0;
}

static char findbase[MAX_OSPATH];
static char findpath[MAX_OSPATH];
static char findpattern[MAX_OSPATH];
static SceUID fdir = -1;

static char *Sys_FindNextEntry(void)
{
	SceIoDirent d;
	if (fdir < 0)
		return NULL;
	while (sceIoDread(fdir, &d) > 0)
	{
		if (!strcmp(d.d_name, ".") || !strcmp(d.d_name, ".."))
			continue;
		if (!*findpattern || glob_match(findpattern, d.d_name))
		{
			Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, d.d_name);
			return findpath;
		}
	}
	return NULL;
}

char *Sys_FindFirst(char *path, unsigned musthave, unsigned canhave)
{
	char *p;
	(void)musthave; (void)canhave;
	if (fdir >= 0)
		Sys_FindClose();

	copy_string(findbase, path, sizeof(findbase));
	if ((p = strrchr(findbase, '/')) != NULL)
	{
		*p = 0;
		copy_string(findpattern, p + 1, sizeof(findpattern));
	}
	else
		copy_string(findpattern, "*", sizeof(findpattern));
	if (!strcmp(findpattern, "*.*"))
		copy_string(findpattern, "*", sizeof(findpattern));

	fdir = sceIoDopen(findbase);
	if (fdir < 0)
		return NULL;
	return Sys_FindNextEntry();
}

char *Sys_FindNext(unsigned musthave, unsigned canhave)
{
	(void)musthave; (void)canhave;
	return Sys_FindNextEntry();
}

void Sys_FindClose(void)
{
	if (fdir >= 0)
		sceIoDclose(fdir);
	fdir = -1;
}

/* ------------------------------------------------------------------------ */
/* First start: controls that make sense on a Vita                          */
/* ------------------------------------------------------------------------ */
static void Vita_WriteDefaultConfig(void)
{
	const char *path = VITA_BASEDIR "/baseq2/config.cfg";
	FILE *f = fopen(path, "r");
	if (f)
	{
		fclose(f);
		return;
	}
	sceIoMkdir(VITA_BASEDIR "/baseq2", 0777);
	f = fopen(path, "w");
	if (!f)
		return;
	fputs("// written by vkQuake2 for PS Vita on the first start\n"
	      "unbindall\n"
	      "bind ESCAPE \"togglemenu\"\n"
	      "bind AUX7 \"+attack\"\n"          // R
	      "bind AUX5 \"+moveup\"\n"          // L
	      "bind AUX1 \"+moveup\"\n"          // cross
	      "bind AUX2 \"+movedown\"\n"        // circle
	      "bind AUX3 \"weapprev\"\n"         // square
	      "bind AUX4 \"weapnext\"\n"         // triangle
	      "bind UPARROW \"invuse\"\n"
	      "bind DOWNARROW \"inven\"\n"
	      "bind LEFTARROW \"invprev\"\n"
	      "bind RIGHTARROW \"invnext\"\n"
	      "bind TAB \"cmd help\"\n"          // select
	      "set lookspring 0\n"
	      "set lookstrafe 0\n"
	      "set vk_msaa 1\n"                 // 2x
	      "set vk_vsync 1\n"
	      "set hand 0\n", f);
	fclose(f);
}

static void Vita_ConfigureVkVita(void)
{
	VkvConfig cfg;
	vkvGetDefaultConfig(&cfg);
	cfg.debugFlags = VKV_DEBUG_LOG_INFO | VKV_DEBUG_LOG_UNSUPPORTED | VKV_DEBUG_DUMP_SHADERS;
	cfg.compatFlags = VKV_COMPAT_SHADER_CACHE; // later starts load the compiled shaders
	cfg.logPath = VITA_BASEDIR "/vkvita.log";
	cfg.shaderDir = VITA_BASEDIR "/shaders";
	cfg.profileInterval = 600;
	vkvSetConfig(&cfg);
	// ux0:data/vkquake2/vkvita.txt can switch on validation and more without a rebuild
	vkvLoadConfigFile(VITA_BASEDIR "/vkvita.txt");
}

/* ------------------------------------------------------------------------ */
/* Entry point                                                              */
/* ------------------------------------------------------------------------ */
int main(int argc, char **argv)
{
	int time, oldtime, newtime;
	char *args[] = { "vkquake2", "+set", "basedir", VITA_BASEDIR, "+set", "cddir", VITA_BASEDIR };
	(void)argc; (void)argv;

	scePowerSetArmClockFrequency(444);
	scePowerSetBusClockFrequency(222);
	scePowerSetGpuClockFrequency(222);
	scePowerSetGpuXbarClockFrequency(166);
	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);

	sceIoMkdir(VITA_BASEDIR, 0777);
	sceIoMkdir(VITA_BASEDIR "/shaders", 0777);
	Vita_ConfigureVkVita();
	Vita_WriteDefaultConfig();
	Vita_Log("vkQuake2 for PS Vita, data in " VITA_BASEDIR "\n");

	// FPU exceptions off, the game divides by zero now and then
	sceKernelChangeThreadVfpException(0x0800009FU, 0x0);

#ifdef VITA_HOST_TEST
	// PC test build against the VkVita mock: VITA_TEST_MAP starts a map,
	// VITA_TEST_FRAMES ends the run
	char *testArgs[16];
	int testArgc = 0, testFrames = getenv("VITA_TEST_FRAMES") ? atoi(getenv("VITA_TEST_FRAMES")) : 300;
	for (unsigned k = 0; k < sizeof(args) / sizeof(args[0]); k++)
		testArgs[testArgc++] = args[k];
	if (getenv("VITA_TEST_MAP"))
	{
		testArgs[testArgc++] = "+map";
		testArgs[testArgc++] = getenv("VITA_TEST_MAP");
	}
	if (getenv("VITA_TEST_ARGS"))
	{
		// space separated, for example "+set developer 1"
		char *a = strdup(getenv("VITA_TEST_ARGS")), *tok = strtok(a, " ");
		while (tok && testArgc < 16)
		{
			testArgs[testArgc++] = tok;
			tok = strtok(NULL, " ");
		}
	}
	Qcommon_Init(testArgc, testArgs);
#else
	Qcommon_Init(sizeof(args) / sizeof(args[0]), args);
#endif
	oldtime = Sys_Milliseconds();
	for (;;)
	{
#ifdef VITA_HOST_TEST
		if (testFrames-- <= 0)
			Sys_Quit();
#endif
		do
		{
			newtime = Sys_Milliseconds();
			time = newtime - oldtime;
		} while (time < 1);
		Qcommon_Frame(time);
		oldtime = newtime;
	}
	return 0;
}
