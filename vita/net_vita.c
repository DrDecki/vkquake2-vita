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
// net_vita.c -- loopback only for now: single player and the local server
// work, packets to other machines are dropped. Taken from linux/net_udp.c.

#include "../qcommon/qcommon.h"

netadr_t net_local_adr;

#define MAX_LOOPBACK 4

typedef struct
{
	byte data[MAX_MSGLEN];
	int datalen;
} loopmsg_t;

typedef struct
{
	loopmsg_t msgs[MAX_LOOPBACK];
	int get, send;
} loopback_t;

static loopback_t loopbacks[2];

static unsigned short net_htons(unsigned short v)
{
	return (unsigned short)((v << 8) | (v >> 8));
}

qboolean NET_CompareAdr(netadr_t a, netadr_t b)
{
	if (a.type != b.type)
		return false;
	if (a.type == NA_LOOPBACK)
		return true;
	return a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3] && a.port == b.port;
}

qboolean NET_CompareBaseAdr(netadr_t a, netadr_t b)
{
	if (a.type != b.type)
		return false;
	if (a.type == NA_LOOPBACK)
		return true;
	if (a.type == NA_IP)
		return a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3];
	if (a.type == NA_IPX)
		return memcmp(a.ipx, b.ipx, 10) == 0;
	return false;
}

char *NET_AdrToString(netadr_t a)
{
	static char s[64];
	if (a.type == NA_LOOPBACK)
		Com_sprintf(s, sizeof(s), "loopback");
	else
		Com_sprintf(s, sizeof(s), "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], net_htons(a.port));
	return s;
}

char *NET_BaseAdrToString(netadr_t a)
{
	static char s[64];
	Com_sprintf(s, sizeof(s), "%i.%i.%i.%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3]);
	return s;
}

// localhost, or a dotted address with an optional :port
qboolean NET_StringToAdr(char *s, netadr_t *a)
{
	int ip[4], port = PORT_SERVER, n;

	memset(a, 0, sizeof(*a));
	if (!strcmp(s, "localhost") || !strcmp(s, "loopback"))
	{
		a->type = NA_LOOPBACK;
		return true;
	}
	n = sscanf(s, "%d.%d.%d.%d:%d", &ip[0], &ip[1], &ip[2], &ip[3], &port);
	if (n < 4)
		return false;
	a->type = NA_IP;
	for (n = 0; n < 4; n++)
		a->ip[n] = (byte)ip[n];
	a->port = net_htons((unsigned short)port);
	return true;
}

qboolean NET_IsLocalAddress(netadr_t adr)
{
	return adr.type == NA_LOOPBACK;
}

static qboolean NET_GetLoopPacket(netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	int i;
	loopback_t *loop = &loopbacks[sock];

	if (loop->send - loop->get > MAX_LOOPBACK)
		loop->get = loop->send - MAX_LOOPBACK;
	if (loop->get >= loop->send)
		return false;

	i = loop->get & (MAX_LOOPBACK - 1);
	loop->get++;
	memcpy(net_message->data, loop->msgs[i].data, loop->msgs[i].datalen);
	net_message->cursize = loop->msgs[i].datalen;
	memset(net_from, 0, sizeof(*net_from));
	net_from->type = NA_LOOPBACK;
	return true;
}

static void NET_SendLoopPacket(netsrc_t sock, int length, void *data, netadr_t to)
{
	int i;
	loopback_t *loop = &loopbacks[sock ^ 1];
	(void)to;

	i = loop->send & (MAX_LOOPBACK - 1);
	loop->send++;
	memcpy(loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
}

qboolean NET_GetPacket(netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	return NET_GetLoopPacket(sock, net_from, net_message);
}

void NET_SendPacket(netsrc_t sock, int length, void *data, netadr_t to)
{
	if (to.type == NA_LOOPBACK)
		NET_SendLoopPacket(sock, length, data, to);
}

void NET_Config(qboolean multiplayer)
{
	static qboolean told;
	if (multiplayer && !told)
	{
		told = true;
		Com_Printf("Network play is not available yet, only the local server works.\n");
	}
}

void NET_Init(void)
{
	memset(&net_local_adr, 0, sizeof(net_local_adr));
	net_local_adr.type = NA_LOOPBACK;
}

void NET_Shutdown(void)
{
}

void NET_Sleep(int msec)
{
	(void)msec;
}
