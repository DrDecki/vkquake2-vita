#include <psp2/audioout.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>

#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void port_path(char *out, size_t cap, const char *p) {
	const char *root = getenv("MOCK_ROOT") ? getenv("MOCK_ROOT") : ".";
	const char *colon = strchr(p, ':');
	if (colon) snprintf(out, cap, "%s/%.*s%s%s", root, (int)(colon - p), p, colon[1] == '/' ? "" : "/", colon + 1);
	else snprintf(out, cap, "%s/%s", root, p);
}

FILE *mock_fopen(const char *path, const char *mode) {
	char p[1024];
	port_path(p, sizeof(p), path);
	return fopen(p, mode);
}

int scePowerSetArmClockFrequency(int f) { (void)f; return 0; }
int scePowerSetBusClockFrequency(int f) { (void)f; return 0; }
int scePowerSetGpuClockFrequency(int f) { (void)f; return 0; }
int scePowerSetGpuXbarClockFrequency(int f) { (void)f; return 0; }
int sceKernelChangeThreadVfpException(SceUInt32 clear, SceUInt32 set) { (void)clear; (void)set; return 0; }

int sceIoGetstat(const char *file, SceIoStat *st) {
	char p[1024];
	struct stat s;
	port_path(p, sizeof(p), file);
	if (stat(p, &s) < 0) return (int)0x80010002;
	memset(st, 0, sizeof(*st));
	st->st_size = s.st_size;
	return 0;
}

#define MAX_DIRS 8
static DIR *g_dirs[MAX_DIRS];
SceUID sceIoDopen(const char *dirname) {
	char p[1024];
	port_path(p, sizeof(p), dirname);
	for (int i = 0; i < MAX_DIRS; i++)
		if (!g_dirs[i]) {
			g_dirs[i] = opendir(p);
			return g_dirs[i] ? 0x1000 + i : (SceUID)0x80010002;
		}
	return (SceUID)0x80010018;
}
int sceIoDread(SceUID fd, SceIoDirent *dir) {
	DIR *d = (fd >= 0x1000 && fd < 0x1000 + MAX_DIRS) ? g_dirs[fd - 0x1000] : NULL;
	struct dirent *e = d ? readdir(d) : NULL;
	if (!e) return 0;
	memset(dir, 0, sizeof(*dir));
	snprintf(dir->d_name, sizeof(dir->d_name), "%s", e->d_name);
	return 1;
}
int sceIoDclose(SceUID fd) {
	if (fd < 0x1000 || fd >= 0x1000 + MAX_DIRS || !g_dirs[fd - 0x1000]) return -1;
	closedir(g_dirs[fd - 0x1000]);
	g_dirs[fd - 0x1000] = NULL;
	return 0;
}

typedef struct { SceKernelThreadEntry entry; pthread_t th; int used; } MockThread;
static MockThread g_threads[8];
static void *thread_main(void *arg) {
	MockThread *t = arg;
	t->entry(0, NULL);
	return NULL;
}
SceUID sceKernelCreateThread(const char *name, SceKernelThreadEntry entry, int prio, SceSize stack, SceUInt attr,
                             int cpu, const SceKernelThreadOptParam *opt) {
	(void)name; (void)prio; (void)stack; (void)attr; (void)cpu; (void)opt;
	for (int i = 0; i < 8; i++)
		if (!g_threads[i].used) {
			g_threads[i].used = 1;
			g_threads[i].entry = entry;
			return 0x2000 + i;
		}
	return -1;
}
int sceKernelStartThread(SceUID id, SceSize len, void *argp) {
	(void)len; (void)argp;
	MockThread *t = &g_threads[id - 0x2000];
	return pthread_create(&t->th, NULL, thread_main, t) ? -1 : 0;
}
int sceKernelWaitThreadEnd(SceUID id, int *stat, SceUInt *timeout) {
	(void)stat; (void)timeout;
	pthread_join(g_threads[id - 0x2000].th, NULL);
	return 0;
}
int sceKernelDeleteThread(SceUID id) {
	g_threads[id - 0x2000].used = 0;
	return 0;
}

static struct { int type, len, freq; } g_ports[4];
static FILE *g_bgm_dump;
int sceAudioOutOpenPort(SceAudioOutPortType type, int len, int freq, SceAudioOutMode mode) {
	(void)mode;
	for (int i = 0; i < 4; i++)
		if (!g_ports[i].len) {
			g_ports[i].type = type; g_ports[i].len = len; g_ports[i].freq = freq;
			if (type == SCE_AUDIO_OUT_PORT_TYPE_BGM && getenv("MOCK_BGM_DUMP") && !g_bgm_dump)
				g_bgm_dump = fopen(getenv("MOCK_BGM_DUMP"), "wb");
			fprintf(stderr, "mock audio: port %d type %d, %d frames at %d Hz\n", i + 1, type, len, freq);
			return i + 1;
		}
	return (int)0x80260109;
}
int sceAudioOutSetVolume(int port, SceAudioOutChannelFlag ch, int *vol) {
	(void)ch;
	if (port >= 1 && port <= 4 && g_ports[port - 1].type == SCE_AUDIO_OUT_PORT_TYPE_BGM)
		fprintf(stderr, "mock audio: BGM volume %d\n", vol[0]);
	return 0;
}
int sceAudioOutOutput(int port, const void *buf) {
	if (port < 1 || port > 4 || !g_ports[port - 1].len) return (int)0x80260102;
	if (!buf) return 0;
	if (g_ports[port - 1].type == SCE_AUDIO_OUT_PORT_TYPE_BGM && g_bgm_dump) {
		fwrite(buf, 4, g_ports[port - 1].len, g_bgm_dump);
		fflush(g_bgm_dump);
	}
	usleep((useconds_t)(1000000LL * g_ports[port - 1].len / g_ports[port - 1].freq));
	return 0;
}
int sceAudioOutReleasePort(int port) {
	if (port >= 1 && port <= 4) g_ports[port - 1].len = 0;
	return 0;
}

#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>

static int net_err(void) {
	int e = errno == EAGAIN || errno == EWOULDBLOCK ? SCE_NET_EWOULDBLOCK
	      : errno == ECONNREFUSED                  ? SCE_NET_ECONNREFUSED
	                                               : 0x7F;
	return (int)(0x80410100u | (unsigned)e);
}
static void to_posix(const SceNetSockaddr *a, struct sockaddr_in *p) {
	const SceNetSockaddrIn *s = (const SceNetSockaddrIn *)a;
	memset(p, 0, sizeof(*p));
	p->sin_family = AF_INET;
	p->sin_port = s->sin_port;
	p->sin_addr.s_addr = s->sin_addr.s_addr;
}
int sceSysmoduleIsLoaded(SceSysmoduleModuleId id) { (void)id; return SCE_SYSMODULE_LOADED; }
int sceSysmoduleLoadModule(SceSysmoduleModuleId id) { (void)id; return 0; }
int sceNetShowNetstat(void) { return (int)SCE_NET_ERROR_ENOTINIT; }
int sceNetInit(SceNetInitParam *p) { return p && p->memory && p->size > 0 ? 0 : -1; }
int sceNetTerm(void) { return 0; }
int sceNetCtlInit(void) { return 0; }
void sceNetCtlTerm(void) {}
int sceNetCtlInetGetInfo(int code, SceNetCtlInfo *info) {
	if (code != SCE_NETCTL_INFO_GET_IP_ADDRESS) return -1;
	snprintf(info->ip_address, sizeof(info->ip_address), "127.0.0.1");
	return 0;
}
int sceNetSocket(const char *name, int domain, int type, int protocol) {
	(void)name;
	if (domain != SCE_NET_AF_INET || type != SCE_NET_SOCK_DGRAM) return -1;
	int s = socket(AF_INET, SOCK_DGRAM, protocol);
	return s < 0 ? net_err() : s;
}
int sceNetSetsockopt(int s, int level, int opt, const void *val, unsigned int len) {
	(void)len;
	if (level != SCE_NET_SOL_SOCKET) return -1;
	if (opt == SCE_NET_SO_NBIO) return fcntl(s, F_SETFL, *(const int *)val ? O_NONBLOCK : 0) < 0 ? net_err() : 0;
	if (opt == SCE_NET_SO_BROADCAST) return setsockopt(s, SOL_SOCKET, SO_BROADCAST, val, sizeof(int)) < 0 ? net_err() : 0;
	return -1;
}
int sceNetBind(int s, const SceNetSockaddr *addr, unsigned int len) {
	(void)len;
	struct sockaddr_in p;
	to_posix(addr, &p);
	return bind(s, (struct sockaddr *)&p, sizeof(p)) < 0 ? net_err() : 0;
}
int sceNetSendto(int s, const void *msg, unsigned int len, int flags, const SceNetSockaddr *to, unsigned int tolen) {
	(void)flags; (void)tolen;
	struct sockaddr_in p;
	to_posix(to, &p);
	ssize_t r = sendto(s, msg, len, 0, (struct sockaddr *)&p, sizeof(p));
	return r < 0 ? net_err() : (int)r;
}
int sceNetRecvfrom(int s, void *buf, unsigned int len, int flags, SceNetSockaddr *from, unsigned int *fromlen) {
	(void)flags;
	struct sockaddr_in p;
	socklen_t pl = sizeof(p);
	ssize_t r = recvfrom(s, buf, len, 0, (struct sockaddr *)&p, &pl);
	if (r < 0) return net_err();
	SceNetSockaddrIn *o = (SceNetSockaddrIn *)from;
	memset(o, 0, sizeof(*o));
	o->sin_len = sizeof(*o);
	o->sin_family = SCE_NET_AF_INET;
	o->sin_port = p.sin_port;
	o->sin_addr.s_addr = p.sin_addr.s_addr;
	if (fromlen) *fromlen = sizeof(*o);
	return (int)r;
}
int sceNetSocketClose(int s) { return close(s) < 0 ? net_err() : 0; }
