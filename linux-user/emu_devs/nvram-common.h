#ifndef _NVRAM_COMMON_H_
#define _NVRAM_COMMON_H_

#ifdef DEBUG
#define DEBUG_PRINTF(format,...) fprintf(stderr,format,## __VA_ARGS__)
#else
#define DEBUG_PRINTF(format,...)
#endif

#define NVRAM_SIZE 0x10000
#define NVRAM_DEV_PATH "/emu_nvram.ini"

int nvram_init(void);
char *nvram_get(const char *key);
int nvram_set(const char *key, const char *value);
int nvram_getall(char *buf, size_t len);

#endif