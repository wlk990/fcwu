/*
 * Block device emulated on standard files
 *
 * Copyright (c) 2017 Christopher Haster
 * Distributed under the MIT license
 */
#include "emubd/lfs_emubd.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>


// Block device emulated on existing filesystem
int lfs_emubd_create(const struct lfs_config *cfg, const char *path) {
    lfs_emubd_t *emu = cfg->context;
    emu->cfg.read_size   = cfg->read_size;
    emu->cfg.prog_size   = cfg->prog_size;
    emu->cfg.block_size  = cfg->block_size;
    emu->cfg.block_count = cfg->block_count;

    // Allocate buffer for creating children files
    size_t pathlen = strlen(path);
    emu->path = malloc(pathlen + 1 + LFS_NAME_MAX + 1);
    if (!emu->path) {
        return -ENOMEM;
    }

    strcpy(emu->path, path);
    emu->path[pathlen] = '/';
    emu->child = &emu->path[pathlen+1];
    memset(emu->child, '\0', LFS_NAME_MAX+1);

    // Create directory if it doesn't exist
    int err = mkdir(path, 0777);
    if (err && errno != EEXIST) {
        return -errno;
    }

    // Load stats to continue incrementing
    snprintf(emu->child, LFS_NAME_MAX, "stats");
    FILE *f = fopen(emu->path, "r");
    if (!f) {
        return -errno;
    }

    size_t res = fread(&emu->stats, sizeof(emu->stats), 1, f);
    if (res < 1) {
        return -errno;
    }

    err = fclose(f);
    if (err) {
        return -errno;
    }

    return 0;
}

void lfs_emubd_destroy(const struct lfs_config *cfg) {
    lfs_emubd_sync(cfg);

    lfs_emubd_t *emu = cfg->context;
    free(emu->path);
}

int lfs_emubd_read(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, lfs_size_t size, void *buffer) {
    lfs_emubd_t *emu = cfg->context;
    uint8_t *data = buffer;

    // Check if read is valid
    assert(off  % cfg->read_size == 0);
    assert(size % cfg->read_size == 0);
    assert(block < cfg->block_count);

    // Zero out buffer for debugging
    memset(data, 0, size);

    // Read data
    snprintf(emu->child, LFS_NAME_MAX, "%x", block);

    FILE *f = fopen(emu->path, "rb");
    if (!f && errno != ENOENT) {
        return -errno;
    }

    if (f) {
        int err = fseek(f, off, SEEK_SET);
        if (err) {
            return -errno;
        }

        size_t res = fread(data, 1, size, f);
        if (res < size && !feof(f)) {
            return -errno;
        }

        err = fclose(f);
        if (err) {
            return -errno;
        }
    }

    emu->stats.read_count += 1;
    return 0;
}

int lfs_emubd_prog(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, lfs_size_t size, const void *buffer) {
    lfs_emubd_t *emu = cfg->context;
    const uint8_t *data = buffer;

    // Check if write is valid
    assert(off  % cfg->prog_size == 0);
    assert(size % cfg->prog_size == 0);
    assert(block < cfg->block_count);

    // Program data
    snprintf(emu->child, LFS_NAME_MAX, "%x", block);

    FILE *f = fopen(emu->path, "r+b");
    if (!f && errno == ENOENT) {
        f = fopen(emu->path, "w+b");
        if (!f) {
            return -errno;
        }
    }

    int err = fseek(f, off, SEEK_SET);
    if (err) {
        return -errno;
    }

    size_t res = fwrite(data, 1, size, f);
    if (res < size) {
        return -errno;
    }

    err = fclose(f);
    if (err) {
        return -errno;
    }

    emu->stats.prog_count += 1;
    return 0;
}

int lfs_emubd_erase(const struct lfs_config *cfg, lfs_block_t block) {
    lfs_emubd_t *emu = cfg->context;

    // Check if erase is valid
    assert(block < cfg->block_count);

    // Erase the block
    snprintf(emu->child, LFS_NAME_MAX, "%x", block);
    struct stat st;
    int err = stat(emu->path, &st);
    if (err && errno != ENOENT) {
        return -errno;
    }

    if (!err && S_ISREG(st.st_mode)) {
        int err = unlink(emu->path);
        if (err) {
            return -errno;
        }
    }

    emu->stats.erase_count += 1;
    return 0;
}

int lfs_emubd_sync(const struct lfs_config *cfg) {
    lfs_emubd_t *emu = cfg->context;

    // Just write out info/stats for later lookup
    snprintf(emu->child, LFS_NAME_MAX, "config");
    FILE *f = fopen(emu->path, "w");
    if (!f) {
        return -errno;
    }

    size_t res = fwrite(&emu->cfg, sizeof(emu->cfg), 1, f);
    if (res < 1) {
        return -errno;
    }

    int err = fclose(f);
    if (err) {
        return -errno;
    }

    snprintf(emu->child, LFS_NAME_MAX, "stats");
    f = fopen(emu->path, "w");
    if (!f) {
        return -errno;
    }

    res = fwrite(&emu->stats, sizeof(emu->stats), 1, f);
    if (res < 1) {
        return -errno;
    }

    err = fclose(f);
    if (err) {
        return -errno;
    }

    return 0;
}

