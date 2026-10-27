/*
Copyright (C) 1997-2001 Id Software, Inc.
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
// in_vita.c -- buttons become key events, the sticks move and look.
//
// Buttons are sent as keys so they can be rebound in the menu and the
// console: START = ESCAPE, SELECT = TAB, d-pad = arrows, cross = AUX1,
// circle = AUX2, square = AUX3, triangle = AUX4, L = AUX5, R = AUX7.
// In menus and the console cross is ENTER and circle is ESCAPE, because the
// main menu only reacts to those.

#include <math.h>
#include <psp2/ctrl.h>

#include "../client/client.h"
#include "vita.h"

extern unsigned sys_frame_time;

cvar_t *in_joystick;
static cvar_t *in_lookspeed;     // degrees per second at full right stick
static cvar_t *in_lookinvert;    // 1 = pushing up looks down
static cvar_t *in_deadzone;      // 0..127

static uint32_t old_buttons;
static int lx, ly, rx, ry;       // -128..127 after the dead zone

typedef struct { uint32_t button; int key; int menu_key; } buttonmap_t;
static const buttonmap_t button_map[] = {
	{ SCE_CTRL_START,    K_ESCAPE,     K_ESCAPE },
	{ SCE_CTRL_SELECT,   K_TAB,        K_TAB },
	{ SCE_CTRL_UP,       K_UPARROW,    K_UPARROW },
	{ SCE_CTRL_DOWN,     K_DOWNARROW,  K_DOWNARROW },
	{ SCE_CTRL_LEFT,     K_LEFTARROW,  K_LEFTARROW },
	{ SCE_CTRL_RIGHT,    K_RIGHTARROW, K_RIGHTARROW },
	{ SCE_CTRL_CROSS,    K_AUX1,       K_ENTER },
	{ SCE_CTRL_CIRCLE,   K_AUX2,       K_ESCAPE },
	{ SCE_CTRL_SQUARE,   K_AUX3,       K_AUX3 },
	{ SCE_CTRL_TRIANGLE, K_AUX4,       K_AUX4 },
	{ SCE_CTRL_LTRIGGER, K_AUX5,       K_AUX5 },
	{ SCE_CTRL_RTRIGGER, K_AUX7,       K_AUX7 },
};
#define NUM_BUTTONS (int)(sizeof(button_map) / sizeof(button_map[0]))

// the key each button sent when it went down, so the release matches it
// even if the menu opened or closed in between
static int sent_key[NUM_BUTTONS];

static int stick(unsigned char raw)
{
	int v = (int)raw - 128;
	int dz = in_deadzone ? (int)in_deadzone->value : 20;
	if (v > -dz && v < dz)
		return 0;
	// rescale so the edge of the dead zone is zero
	if (v > 0)
		return (v - dz) * 127 / (127 - dz);
	return (v + dz) * 128 / (128 - dz);
}

void Sys_SendKeyEvents(void)
{
	SceCtrlData pad;
	uint32_t changed;
	int i, now;

	sys_frame_time = Sys_Milliseconds();
	if (sceCtrlPeekBufferPositive(0, &pad, 1) < 0)
		return;
#ifdef VITA_HOST_TEST
	// the mock presses buttons for the VkVita samples, which would open the menu
	if (getenv("VITA_TEST_NOPAD"))
	{
		memset(&pad, 0, sizeof(pad));
		pad.lx = pad.ly = pad.rx = pad.ry = 128;
	}
#endif

	now = Sys_Milliseconds();
	changed = pad.buttons ^ old_buttons;
	for (i = 0; i < NUM_BUTTONS; i++)
	{
		if (!(changed & button_map[i].button))
			continue;
		if (pad.buttons & button_map[i].button)
		{
			sent_key[i] = cls.key_dest == key_game ? button_map[i].key : button_map[i].menu_key;
			Key_Event(sent_key[i], true, now);
		}
		else if (sent_key[i])
		{
			Key_Event(sent_key[i], false, now);
			sent_key[i] = 0;
		}
	}
	old_buttons = pad.buttons;

	lx = stick(pad.lx);
	ly = stick(pad.ly);
	rx = stick(pad.rx);
	ry = stick(pad.ry);
}

void IN_Init(void)
{
	in_joystick = Cvar_Get("in_joystick", "1", CVAR_ARCHIVE);
	in_lookspeed = Cvar_Get("in_lookspeed", "160", CVAR_ARCHIVE);
	in_lookinvert = Cvar_Get("in_lookinvert", "0", CVAR_ARCHIVE);
	in_deadzone = Cvar_Get("in_deadzone", "20", CVAR_ARCHIVE);
}

void IN_Shutdown(void)
{
}

void IN_Commands(void)
{
}

void IN_Frame(void)
{
}

void IN_Activate(qboolean active)
{
	(void)active;
}

void IN_Move(usercmd_t *cmd)
{
	float speed, look;

	if (cls.key_dest != key_game || !cl.refresh_prepped)
		return;

	// left stick: walk and strafe, running follows the run key like the keyboard
	speed = ((in_speed.state & 1) ^ (int)cl_run->value) ? 2.0f : 1.0f;
	cmd->forwardmove -= (short)(speed * cl_forwardspeed->value * ly / 128.0f);
	cmd->sidemove += (short)(speed * cl_sidespeed->value * lx / 128.0f);

	// right stick: look, with a curve so small movements aim precisely
	look = in_lookspeed->value * cls.frametime;
	if (rx)
		cl.viewangles[YAW] -= look * (rx / 128.0f) * fabsf(rx / 128.0f);
	if (ry)
	{
		float p = look * (ry / 128.0f) * fabsf(ry / 128.0f);
		cl.viewangles[PITCH] += in_lookinvert->value ? -p : p;
	}
}
