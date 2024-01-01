//
// /dev/ramdisk device driver
// 
// Copyright (C) 2024 Tom Cully
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
//
// Original by Oleg Kutkov (https://olegkutkov.me/2020/02/10/linux-block-device-driver)
// Adapted for linux-6.6.7-0-lts by Tom Cully 2023
//
// Tested under linux 6.6.7-0-lts
//
#ifndef RAMDISK_HEADERS
#define RAMDISK_HEADERS

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/blk-mq.h>
#include <linux/hdreg.h>

#define TOTAL_SECTORS 204800

// Init & Exit
static int __init ramdisk_driver_init(void);
static void __exit ramdisk_driver_exit(void);
static void ramdisk_cleanup(void);

// Device functions
static int ramdisk_open(struct gendisk *disk, blk_mode_t mode);
static void ramdisk_release(struct gendisk *disk);
static int ramdisk_ioctl(struct block_device *bdev, fmode_t mode, unsigned cmd, unsigned long arg);

// Queue Functions
static int ramdisk_process_request(struct request *rq, unsigned int *nr_bytes);
static blk_status_t ramdisk_queue_request(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data* bd);

#endif