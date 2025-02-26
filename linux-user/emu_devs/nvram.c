#include <stdio.h>

#include "nvram.h"

static char *nvram;
static size_t nvram_offset = 0;


static DevInfo nvram_dev_info = {
    .ops = {
        .read = op_read,
        .write = op_write,
        .ioctl = op_ioctl,
    }
};

static TargetFdTrans nvram_fd_trans = {
    .dev_info = &nvram_dev_info,
};

static ssize_t op_read(unsigned int fd, char * buf, size_t count) {
    if (*buf) {
        nvram_get(buf);
    } else {
        nvram_getall(buf, count);
    }
    return 0;
}

static ssize_t op_write(unsigned int fd, char * buf, size_t count) {
    strchr(buf, '=');
    nvram_set(buf, strchr(buf, '=') + 1);
    return count;
}

static int op_ioctl(int fd, unsigned long request, void *arg) {
    // TODO
    return 0;
}
