#ifndef _NVRAM_H_
#define _NVRAM_H_

#include "../cpu_loop-common.h"
#include "../fd-trans.h"

#define NVRAM_SIZE 0x10000
#define NVRAM_DEV_PATH "/emu_nvram.ini"

int nvram_open_dev(CPUArchState *cpu_env, int fd);

#endif // !_NVRAM_H_