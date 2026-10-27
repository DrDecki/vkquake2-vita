/*
Copyright (C) 1997-2001 Id Software, Inc.
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
// vid_vita.c -- the Vulkan renderer is linked in, so there is no library to
// load. Based on linux/vid_so.c.

#include "../client/client.h"
#include "vita.h"

refexport_t GetRefAPI(refimport_t rimp);

refexport_t re;

// the renderer is linked in and already defines these, they are the same cvars
extern cvar_t *vid_gamma;
extern cvar_t *vid_ref;
extern cvar_t *vid_fullscreen;
extern cvar_t *vid_refresh;
extern cvar_t *viewsize;

cvar_t *vid_xpos;
cvar_t *vid_ypos;
cvar_t *vid_hudscale;
cvar_t *r_customwidth;
cvar_t *r_customheight;

viddef_t viddef;
qboolean reflib_active = false;

#define MAXPRINTMSG 8192

void VID_Printf(int print_level, char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];
	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);
	if (print_level == PRINT_ALL)
		Com_Printf("%s", msg);
	else
		Com_DPrintf("%s", msg);
}

void VID_Error(int err_level, char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];
	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);
	Com_Error(err_level, "%s", msg);
}

void VID_Restart_f(void)
{
	vid_ref->modified = true;
}

// every mode is the Vita screen
qboolean VID_GetModeInfo(int *width, int *height, int mode)
{
	(void)mode;
	*width = VITA_SCREEN_WIDTH;
	*height = VITA_SCREEN_HEIGHT;
	return true;
}

void VID_NewWindow(int width, int height)
{
	char hudscale[4];
	int wscale, hscale;

	viddef.width = width;
	viddef.height = height;
	wscale = viddef.width / 800;
	hscale = viddef.height / 480;
	if (wscale > hscale)
		wscale = hscale;
	if (wscale < 1)
		wscale = 1;
	snprintf(hudscale, sizeof(hudscale), "%2d", wscale);
	vid_hudscale = Cvar_Set("hudscale", hudscale);
}

static void VID_FreeReflib(void)
{
	memset(&re, 0, sizeof(re));
	reflib_active = false;
}

static qboolean VID_LoadRefresh(void)
{
	refimport_t ri;

	if (reflib_active)
	{
		re.Shutdown();
		VID_FreeReflib();
	}

	Com_Printf("------- Starting the Vulkan renderer -------\n");

	ri.Cmd_AddCommand = Cmd_AddCommand;
	ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
	ri.Cmd_Argc = Cmd_Argc;
	ri.Cmd_Argv = Cmd_Argv;
	ri.Cmd_ExecuteText = Cbuf_ExecuteText;
	ri.Con_Printf = VID_Printf;
	ri.Sys_Error = VID_Error;
	ri.FS_LoadFile = FS_LoadFile;
	ri.FS_FreeFile = FS_FreeFile;
	ri.FS_Gamedir = FS_Gamedir;
	ri.Cvar_Get = Cvar_Get;
	ri.Cvar_Set = Cvar_Set;
	ri.Cvar_SetValue = Cvar_SetValue;
	ri.Vid_GetModeInfo = VID_GetModeInfo;
	ri.Vid_MenuInit = VID_MenuInit;
	ri.Vid_NewWindow = VID_NewWindow;

	re = GetRefAPI(ri);
	if (re.api_version != API_VERSION)
	{
		VID_FreeReflib();
		Com_Error(ERR_FATAL, "renderer has incompatible api_version");
	}

	if (!re.Init(0, 0))
	{
		re.Shutdown();
		VID_FreeReflib();
		return false;
	}

	Key_ClearStates();
	Com_Printf("------------------------------------\n");
	reflib_active = true;
	return true;
}

void VID_CheckChanges(void)
{
	if (vid_ref->modified)
		S_StopAllSounds();

	while (vid_ref->modified)
	{
		if (strcmp("vk", vid_ref->string))
		{
			Cvar_Set("vid_ref", "vk");
			Com_Printf("Only the Vulkan renderer is available on the Vita.\n");
		}
		vid_ref->modified = false;
		vid_fullscreen->modified = true;
		cl.refresh_prepped = false;
		cls.disable_screen = true;

		if (!VID_LoadRefresh())
			Com_Error(ERR_FATAL, "Couldn't start the Vulkan renderer, see " VITA_BASEDIR "/vkvita.log");

		cls.disable_screen = false;
	}

	if (vid_refresh->modified)
	{
		vid_refresh->modified = false;
		cl.refresh_prepped = false;
	}
}

void VID_Init(void)
{
	vid_ref = Cvar_Get("vid_ref", "vk", CVAR_ARCHIVE);
	vid_xpos = Cvar_Get("vid_xpos", "0", CVAR_ARCHIVE);
	vid_ypos = Cvar_Get("vid_ypos", "0", CVAR_ARCHIVE);
	vid_fullscreen = Cvar_Get("vid_fullscreen", "1", CVAR_ARCHIVE);
	vid_refresh = Cvar_Get("vid_refresh", "0", CVAR_NOSET);
	vid_gamma = Cvar_Get("vid_gamma", "1", CVAR_ARCHIVE);
	r_customwidth = Cvar_Get("r_customwidth", "960", CVAR_ARCHIVE);
	r_customheight = Cvar_Get("r_customheight", "544", CVAR_ARCHIVE);
	viewsize = Cvar_Get("viewsize", "100", CVAR_ARCHIVE);

	Cmd_AddCommand("vid_restart", VID_Restart_f);

	VID_CheckChanges();
}

void VID_Shutdown(void)
{
	if (reflib_active)
	{
		re.Shutdown();
		VID_FreeReflib();
	}
}
