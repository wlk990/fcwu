/*
 * Block device emulated in a file
 *
 * Copyright (c) 2017, Arm Limited. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef LFS_FILEBD_H
#define LFS_FILEBD_H

#include "lfs.h"
#include "lfs_util.h"

#ifdef __cplusplus
extern "C"
{
#endif

// filebd config (optional)
struct lfs_filebd_config {
    // 8-bit erase value to simulate erasing with. -1 indicates no erase
    // occurs, which is still a valid block device
    int32_t erase_value;
};

// filebd state
typedef struct lfs_filebd {
    int fd;
    const struct lfs_filebd_config *cfg;
} lfs_filebd_t;


// Create a file block device using the geometry in lfs_config
int lfs_filebd_create(const struct lfs_config *cfg, const char *path);
int lfs_filebd_createcfg(const struct lfs_config *cfg, const char *path,
        const struct lfs_filebd_config *ramcfg);

// Clean up memory associated with block device
void lfs_filebd_destroy(const struct lfs_config *cfg);

// Read a block
int lfs_filebd_read(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, void *buffer, lfs_size_t size);

// Program a block
//
// The block must have previously been erased.
int lfs_filebd_prog(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, const void *buffer, lfs_size_t size);

// Erase a block
//
// A block must be erased before being programmed. The
// state of an erased block is undefined.
int lfs_filebd_erase(const struct lfs_config *cfg, lfs_block_t block);

// Sync the block device
int lfs_filebd_sync(const struct lfs_config *cfg);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
