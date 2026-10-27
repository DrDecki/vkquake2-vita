/*
Copyright (C) 2026 DrDecki

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

*/

#ifndef VITA_H
#define VITA_H

#define VITA_BASEDIR "ux0:data/vkquake2"

#define VITA_HEAP_SIZE (160 * 1024 * 1024)

#define VITA_SCREEN_WIDTH  960
#define VITA_SCREEN_HEIGHT 544

void Vita_Log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
