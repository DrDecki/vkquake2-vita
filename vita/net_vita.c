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
// net_vita.c -- UDP over the Vita network stack (sceNet), plus the loopback
// buffers for the local server. Follows linux/net_udp.c. Addresses are
// dotted IPs; host names are not resolved.

#include "../qcommon/qcommon.h"

#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>

netadr_t net_local_adr;

#define MAX_LOOPBACK 4
#define NET_POOL_SIZE (256 * 1024)

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
static int ip_sockets[2] = { -1, -1 };
static qboolean net_ready;
static char net_pool[NET_POOL_SIZE] __attribute__((aligned(64)));

static unsigned short net_htons(unsigned short v)
{
	return (unsigned short)((v << 8) | (v >> 8));
}

// sceNet errors are 0x804101xx with the errno in the low byte
static int net_errno(int r)
{
	return r & 0xFF;
}

static void NetadrToSockadr(const netadr_t *a, SceNetSockaddrIn *s)
{
	memset(s, 0, sizeof(*s));
	s->sin_len = sizeof(*s);
	s->sin_family = SCE_NET_AF_INET;
	s->sin_port = a->port;
	if (a->type == NA_BROADCAST)
		s->sin_addr.s_addr = SCE_NET_INADDR_BROADCAST;
	else
		memcpy(&s->sin_addr.s_addr, a->ip, 4);
}

static void SockadrToNetadr(const SceNetSockaddrIn *s, netadr_t *a)
{
	memset(a, 0, sizeof(*a));
	a->type = NA_IP;
	memcpy(a->ip, &s->sin_addr.s_addr, 4);
	a->port = s->sin_port;
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
	int ip[4], port = 0, n;

	memset(a, 0, sizeof(*a));
	if (!strcmp(s, "localhost") || !strcmp(s, "loopback"))
	{
		a->type = NA_LOOPBACK;
		return true;
	}
	n = sscanf(s, "%d.%d.%d.%d:%d", &ip[0], &ip[1], &ip[2], &ip[3], &port);
	if (n < 4)
	{
		Com_Printf("%s: host names are not resolved on the Vita, use an IP address\n", s);
		return false;
	}
	for (n = 0; n < 4; n++)
	{
		if (ip[n] < 0 || ip[n] > 255)
			return false;
		a->ip[n] = (byte)ip[n];
	}
	a->type = NA_IP;
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
	SceNetSockaddrIn from;
	unsigned int fromlen;
	int ret;

	if (NET_GetLoopPacket(sock, net_from, net_message))
		return true;
	if (ip_sockets[sock] < 0)
		return false;

	for (;;)
	{
		fromlen = sizeof(from);
		ret = sceNetRecvfrom(ip_sockets[sock], net_message->data, net_message->maxsize, 0, (SceNetSockaddr *)&from,
		                     &fromlen);
		if (ret < 0)
		{
			int e = net_errno(ret);
			if (e != SCE_NET_EWOULDBLOCK && e != SCE_NET_ECONNREFUSED)
				Com_Printf("NET_GetPacket: error 0x%08X\n", (unsigned)ret);
			return false;
		}
		SockadrToNetadr(&from, net_from);
		if (ret == net_message->maxsize)
		{
			Com_Printf("Oversize packet from %s\n", NET_AdrToString(*net_from));
			continue;
		}
		net_message->cursize = ret;
		return true;
	}
}

void NET_SendPacket(netsrc_t sock, int length, void *data, netadr_t to)
{
	SceNetSockaddrIn addr;
	int ret;

	if (to.type == NA_LOOPBACK)
	{
		NET_SendLoopPacket(sock, length, data, to);
		return;
	}
	if (to.type == NA_IPX || to.type == NA_BROADCAST_IPX)
		return;
	if (to.type != NA_IP && to.type != NA_BROADCAST)
		Com_Error(ERR_FATAL, "NET_SendPacket: bad address type");
	if (ip_sockets[sock] < 0)
		return;

	NetadrToSockadr(&to, &addr);
	ret = sceNetSendto(ip_sockets[sock], data, length, 0, (SceNetSockaddr *)&addr, sizeof(addr));
	if (ret < 0 && net_errno(ret) != SCE_NET_EWOULDBLOCK)
		Com_Printf("NET_SendPacket: error 0x%08X to %s\n", (unsigned)ret, NET_AdrToString(to));
}

static int NET_Socket(const char *net_interface, int port)
{
	SceNetSockaddrIn address;
	int s, on = 1, r;

	s = sceNetSocket("q2udp", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, SCE_NET_IPPROTO_UDP);
	if (s < 0)
	{
		Com_Printf("ERROR: UDP_OpenSocket: socket 0x%08X\n", (unsigned)s);
		return -1;
	}
	if ((r = sceNetSetsockopt(s, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &on, sizeof(on))) < 0 ||
	    (r = sceNetSetsockopt(s, SCE_NET_SOL_SOCKET, SCE_NET_SO_BROADCAST, &on, sizeof(on))) < 0)
	{
		Com_Printf("ERROR: UDP_OpenSocket: setsockopt 0x%08X\n", (unsigned)r);
		sceNetSocketClose(s);
		return -1;
	}

	memset(&address, 0, sizeof(address));
	address.sin_len = sizeof(address);
	address.sin_family = SCE_NET_AF_INET;
	address.sin_addr.s_addr = SCE_NET_INADDR_ANY;
	if (net_interface && net_interface[0] && Q_stricmp((char *)net_interface, "localhost"))
	{
		netadr_t a;
		if (NET_StringToAdr((char *)net_interface, &a) && a.type == NA_IP)
			memcpy(&address.sin_addr.s_addr, a.ip, 4);
	}
	address.sin_port = port == PORT_ANY ? 0 : net_htons((unsigned short)port);

	if ((r = sceNetBind(s, (SceNetSockaddr *)&address, sizeof(address))) < 0)
	{
		Com_Printf("ERROR: UDP_OpenSocket: bind to port %i 0x%08X\n", port, (unsigned)r);
		sceNetSocketClose(s);
		return -1;
	}
	return s;
}

static void NET_OpenIP(void)
{
	cvar_t *port = Cvar_Get("port", va("%i", PORT_SERVER), CVAR_NOSET);
	cvar_t *ip = Cvar_Get("ip", "localhost", CVAR_NOSET);

	if (!net_ready)
		return;
	if (ip_sockets[NS_SERVER] < 0)
		ip_sockets[NS_SERVER] = NET_Socket(ip->string, (int)port->value);
	if (ip_sockets[NS_CLIENT] < 0)
		ip_sockets[NS_CLIENT] = NET_Socket(ip->string, PORT_ANY);
}

// A single player game only uses the loopback buffers
void NET_Config(qboolean multiplayer)
{
	int i;

	if (!multiplayer)
	{
		for (i = 0; i < 2; i++)
		{
			if (ip_sockets[i] >= 0)
			{
				sceNetSocketClose(ip_sockets[i]);
				ip_sockets[i] = -1;
			}
		}
		return;
	}
	if (!net_ready)
	{
		static qboolean told;
		if (!told)
		{
			told = true;
			Com_Printf("No network: connect the Vita to Wi-Fi and restart the game.\n");
		}
		return;
	}
	NET_OpenIP();
}

void NET_Init(void)
{
	SceNetInitParam param;
	SceNetCtlInfo info;
	int r;

	memset(&net_local_adr, 0, sizeof(net_local_adr));
	net_local_adr.type = NA_LOOPBACK;

	if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NET) != SCE_SYSMODULE_LOADED)
		sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
	// another part of the app may have set the stack up already
	if (sceNetShowNetstat() == SCE_NET_ERROR_ENOTINIT)
	{
		param.memory = net_pool;
		param.size = sizeof(net_pool);
		param.flags = 0;
		r = sceNetInit(&param);
		if (r < 0)
		{
			Com_Printf("sceNetInit failed 0x%08X, network play is off\n", (unsigned)r);
			return;
		}
	}
	sceNetCtlInit();
	net_ready = true;

	if (sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &info) >= 0)
	{
		netadr_t a;
		if (NET_StringToAdr(info.ip_address, &a))
		{
			net_local_adr = a;
			Com_Printf("IP address %s\n", info.ip_address);
		}
	}
	else
		Com_Printf("Not connected to Wi-Fi, only the local server works\n");
}

void NET_Shutdown(void)
{
	NET_Config(false);
	if (net_ready)
	{
		sceNetCtlTerm();
		sceNetTerm();
		net_ready = false;
	}
}

void NET_Sleep(int msec)
{
	(void)msec;
}
