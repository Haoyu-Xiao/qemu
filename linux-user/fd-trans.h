/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FD_TRANS_H
#define FD_TRANS_H

#include "qemu/lockable.h"


typedef abi_long (*TargetFdDataFunc)(void *, size_t);
typedef abi_long (*TargetFdAddrFunc)(void *, abi_ulong, socklen_t);
typedef struct TargetFdTrans {
    TargetFdDataFunc host_to_target_data;
    TargetFdDataFunc target_to_host_data;
    TargetFdAddrFunc target_to_host_addr;
} TargetFdTrans;

typedef struct SockfsCarrierRef {
    int host_fd;
    int refcount;
    bool host_backed;
} SockfsCarrierRef;

extern TargetFdTrans **target_fd_trans;
extern QemuMutex target_fd_trans_lock;

extern unsigned int target_fd_max;
extern SockfsCarrierRef **sockfs_carrier_refs;
extern unsigned int sockfs_carrier_max;

static inline void fd_trans_init(void)
{
    qemu_mutex_init(&target_fd_trans_lock);
    sockfs_carrier_refs = NULL;
    sockfs_carrier_max = 0;
}

static inline void fd_trans_prefork(void)
{
    qemu_mutex_lock(&target_fd_trans_lock);
}

static inline void fd_trans_postfork(void)
{
    qemu_mutex_unlock(&target_fd_trans_lock);
}

static inline TargetFdDataFunc fd_trans_target_to_host_data(int fd)
{
    if (fd < 0) {
        return NULL;
    }

    QEMU_LOCK_GUARD(&target_fd_trans_lock);
    if (fd < target_fd_max && target_fd_trans[fd]) {
        return target_fd_trans[fd]->target_to_host_data;
    }
    return NULL;
}

static inline TargetFdDataFunc fd_trans_host_to_target_data(int fd)
{
    if (fd < 0) {
        return NULL;
    }

    QEMU_LOCK_GUARD(&target_fd_trans_lock);
    if (fd < target_fd_max && target_fd_trans[fd]) {
        return target_fd_trans[fd]->host_to_target_data;
    }
    return NULL;
}

static inline TargetFdAddrFunc fd_trans_target_to_host_addr(int fd)
{
    if (fd < 0) {
        return NULL;
    }

    QEMU_LOCK_GUARD(&target_fd_trans_lock);
    if (fd < target_fd_max && target_fd_trans[fd]) {
        return target_fd_trans[fd]->target_to_host_addr;
    }
    return NULL;
}

static inline void internal_fd_trans_register_unsafe(int fd,
                                                     TargetFdTrans *trans)
{
    unsigned int oldmax;

    if (fd >= target_fd_max) {
        oldmax = target_fd_max;
        target_fd_max = ((fd >> 6) + 1) << 6; /* by slice of 64 entries */
        target_fd_trans = g_renew(TargetFdTrans *,
                                  target_fd_trans, target_fd_max);
        memset((void *)(target_fd_trans + oldmax), 0,
               (target_fd_max - oldmax) * sizeof(TargetFdTrans *));
    }
    target_fd_trans[fd] = trans;
}

static inline void internal_sockfs_carrier_register_unsafe(int fd,
                                                           SockfsCarrierRef *carrier)
{
    unsigned int oldmax;

    if (fd >= sockfs_carrier_max) {
        oldmax = sockfs_carrier_max;
        sockfs_carrier_max = ((fd >> 6) + 1) << 6;
        sockfs_carrier_refs = g_renew(SockfsCarrierRef *,
                                      sockfs_carrier_refs, sockfs_carrier_max);
        memset((void *)(sockfs_carrier_refs + oldmax), 0,
               (sockfs_carrier_max - oldmax) * sizeof(SockfsCarrierRef *));
    }
    sockfs_carrier_refs[fd] = carrier;
}

static inline void internal_sockfs_carrier_unref_unsafe(SockfsCarrierRef *carrier)
{
    if (!carrier) {
        return;
    }

    carrier->refcount--;
    if (carrier->refcount == 0) {
        if (carrier->host_fd >= 0) {
            close(carrier->host_fd);
        }
        g_free(carrier);
    }
}

static inline void internal_sockfs_carrier_unregister_unsafe(int fd)
{
    SockfsCarrierRef *carrier;

    if (fd < 0 || fd >= sockfs_carrier_max) {
        return;
    }

    carrier = sockfs_carrier_refs[fd];
    sockfs_carrier_refs[fd] = NULL;
    internal_sockfs_carrier_unref_unsafe(carrier);
}

static inline void fd_trans_register(int fd, TargetFdTrans *trans)
{
    QEMU_LOCK_GUARD(&target_fd_trans_lock);
    internal_fd_trans_register_unsafe(fd, trans);
}

static inline void internal_fd_trans_unregister_unsafe(int fd)
{
    if (fd >= 0 && fd < target_fd_max) {
        target_fd_trans[fd] = NULL;
    }
}

static inline void fd_trans_unregister(int fd)
{
    if (fd < 0) {
        return;
    }

    QEMU_LOCK_GUARD(&target_fd_trans_lock);
    internal_fd_trans_unregister_unsafe(fd);
    internal_sockfs_carrier_unregister_unsafe(fd);
}

static inline void fd_trans_dup(int oldfd, int newfd)
{
    QEMU_LOCK_GUARD(&target_fd_trans_lock);
    internal_fd_trans_unregister_unsafe(newfd);
    internal_sockfs_carrier_unregister_unsafe(newfd);
    if (oldfd < target_fd_max && target_fd_trans[oldfd]) {
        internal_fd_trans_register_unsafe(newfd, target_fd_trans[oldfd]);
    }
    if (oldfd < sockfs_carrier_max && sockfs_carrier_refs[oldfd]) {
        SockfsCarrierRef *carrier = sockfs_carrier_refs[oldfd];

        carrier->refcount++;
        internal_sockfs_carrier_register_unsafe(newfd, carrier);
    }
}

static inline void sockfs_carrier_register(int fd, int host_fd, bool host_backed)
{
    SockfsCarrierRef *carrier = g_new0(SockfsCarrierRef, 1);

    carrier->host_fd = host_fd;
    carrier->refcount = 1;
    carrier->host_backed = host_backed;

    QEMU_LOCK_GUARD(&target_fd_trans_lock);
    internal_sockfs_carrier_unregister_unsafe(fd);
    internal_sockfs_carrier_register_unsafe(fd, carrier);
}

static inline int sockfs_carrier_hostfd(int fd)
{
    int host_fd = -1;

    if (fd < 0) {
        return -1;
    }

    QEMU_LOCK_GUARD(&target_fd_trans_lock);
    if (fd < sockfs_carrier_max && sockfs_carrier_refs[fd] &&
        sockfs_carrier_refs[fd]->host_backed) {
        host_fd = sockfs_carrier_refs[fd]->host_fd;
    }
    return host_fd;
}

extern TargetFdTrans target_packet_trans;
#ifdef CONFIG_RTNETLINK
extern TargetFdTrans target_netlink_route_trans;
#endif
extern TargetFdTrans target_netlink_audit_trans;
extern TargetFdTrans target_signalfd_trans;
extern TargetFdTrans target_eventfd_trans;
extern TargetFdTrans target_timerfd_trans;
#if (defined(TARGET_NR_inotify_init) && defined(__NR_inotify_init)) || \
    (defined(CONFIG_INOTIFY1) && defined(TARGET_NR_inotify_init1) && \
     defined(__NR_inotify_init1))
extern TargetFdTrans target_inotify_trans;
#endif
#endif
