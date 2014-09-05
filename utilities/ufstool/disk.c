/*
 * Copyright (c) 2002 Juli Mallett.  All rights reserved.
 *
 * This software was written by Juli Mallett <jmallett@FreeBSD.org> for the
 * FreeBSD project.  Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistribution of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistribution in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libufs.h"
#include "internal.h"

int
ufs_disk_close(ufs_t *disk)
{
    close(disk->d_fd);
    if (disk->d_inoblock != NULL) {
        free(disk->d_inoblock);
        disk->d_inoblock = NULL;
    }
    if (disk->d_sbcsum != NULL) {
        free(disk->d_sbcsum);
        disk->d_sbcsum = NULL;
    }
    return (0);
}

int
ufs_disk_open(ufs_t *disk, const char *name)
{
    if (ufs_disk_open_blank(disk, name) == -1) {
        return (-1);
    }
    if (ufs_superblock_read(disk) == -1) {
        fprintf(stderr, "%s: could not read superblock to fill out disk\n", __func__);
        return (-1);
    }
    return (0);
}

int
ufs_disk_open_blank(ufs_t *disk, const char *name)
{
    int fd;

    fd = open(name, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "%s: could not open disk image\n", __func__);
        return (-1);
    }

    memset(disk, 0, sizeof(*disk));
    disk->d_secsize = 1;
    disk->d_ccg = 0;
    disk->d_fd = fd;
    disk->d_inoblock = NULL;
    disk->d_inomin = 0;
    disk->d_inomax = 0;
    disk->d_lcg = 0;
    disk->d_writable = 0;
    disk->d_ufs = 0;
    disk->d_error = NULL;
    disk->d_sbcsum = NULL;
    disk->d_name = name;
    return (0);
}

int
ufs_disk_reopen_writable(ufs_t *disk)
{
    if (! disk->d_writable) {
        close(disk->d_fd);
        disk->d_fd = open(disk->d_name, O_RDWR);
        if (disk->d_fd < 0) {
            fprintf(stderr, "%s: failed to open disk for writing\n", __func__);
            return (-1);
        }
        disk->d_writable = 1;
    }
    return (0);
}
