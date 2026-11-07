/*
 * vkQuake2 for PS Vita: boot screen in the style of a 1997 PC starting up.
 * Drawn through VkVita's custom boot screen hook while the game loads.
 * The BIOS, its vendor and the drives are made up.
 */
#include <stdio.h>
#include <string.h>

#include <vkvita/vkvita.h>

#include "vita.h"

#define COLS 80
#define ROWS 25
#define ORIGIN_X ((960 - COLS * 8) / 2)
#define ORIGIN_Y ((544 - ROWS * 16) / 2)

#define GREY  0xFFAAAAAAu
#define WHITE 0xFFFFFFFFu

static char s_shown[ROWS][COLS + 1];

/* Draws a row only when its text changed, so most frames touch no pixels. */
static void boot_row(uint32_t *px, uint32_t pitch, int row, uint32_t color, const char *text)
{
	char line[COLS + 1];
	snprintf(line, sizeof(line), "%s", text);
	if (!strcmp(line, s_shown[row]))
		return;
	memcpy(s_shown[row], line, sizeof(line));
	int y0 = ORIGIN_Y + row * 16;
	for (int y = y0; y < y0 + 16; y++)
		memset(px + y * pitch + ORIGIN_X, 0, COLS * 8 * 4);
	vkvSplashText(px, pitch, ORIGIN_X, y0, 1, color, line);
}

static void boot_draw(uint32_t *px, uint32_t pitch, uint32_t ms, void *user)
{
	char buf[COLS + 1];
	(void)user;
	if (ms == 0)
		memset(s_shown, 0, sizeof(s_shown));

	boot_row(px, pitch, 0, WHITE, "VkVita Modular BIOS v1.97, (C) 1997 VkVita Systems");
	boot_row(px, pitch, 2, GREY, "Main Processor : 586-class CPU at 233 MHz");

	/* the memory count runs up to 32 MB in 1.2 seconds */
	unsigned kb = ms >= 1200 ? 32768 : (ms * 32768 / 1200) & ~1023u;
	snprintf(buf, sizeof(buf), "Memory Testing : %6uK%s", kb, ms >= 1200 ? " OK" : "");
	boot_row(px, pitch, 3, GREY, buf);

	static const struct { const char *slot, *drive; } drives[4] = {
		{ "Primary Master  ", "STROGG-2100A  2.1 GB" },
		{ "Primary Slave   ", "None" },
		{ "Secondary Master", "CD-ROM 24X" },
		{ "Secondary Slave ", "None" },
	};
	for (int i = 0; i < 4; i++)
	{
		uint32_t t = 1400 + i * 300;
		if (ms < t)
			break;
		snprintf(buf, sizeof(buf), "Detecting IDE %s ... %s", drives[i].slot, ms >= t + 200 ? drives[i].drive : "");
		boot_row(px, pitch, 5 + i, GREY, buf);
	}

	boot_row(px, pitch, ROWS - 1, GREY, ms >= 1400 && ms < 2800 ? "Press DEL to enter SETUP" : "");
	if (ms >= 2800)
	{
		int dots = ms >= 3400 ? 20 : (int)(ms - 2800) / 30;
		snprintf(buf, sizeof(buf), "Verifying DMI Pool Data %.*s", dots, "....................");
		boot_row(px, pitch, 11, GREY, buf);
	}
	if (ms >= 3500)
	{
		/* a blinking cursor until the game shows its first frame */
		snprintf(buf, sizeof(buf), "Starting Quake II ...%s", (ms / 500) & 1 ? "" : " _");
		boot_row(px, pitch, 13, WHITE, buf);
	}
}

void Vita_SetBootScreen(void)
{
	vkvSetSplashDraw(boot_draw, NULL);
}
