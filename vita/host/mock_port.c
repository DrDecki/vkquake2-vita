/*
 * The system calls vkQuake2 makes beyond what the VkVita mock already has,
 * for the PC test build. Paths like ux0:data/x go to $MOCK_ROOT/ux0/data/x.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <psp2/audioout.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>

/* after the Vita headers: glibc turns st_mtime into a macro */
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

/* newlib on the Vita understands ux0: paths, glibc does not */
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

/* threads: one pthread per kernel thread */
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

/* audio: each output call takes as long as its samples would play. With
 * MOCK_BGM_DUMP set the BGM port writes its samples there as raw s16 stereo. */
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
