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
#include "ramdisk.h"

static int dev_major = 0;

struct ramdisk {
    sector_t capacity_sectors;
    u8 *data;   // Data buffer to emulate real storage device
    struct blk_mq_tag_set *tag_set;
    struct gendisk *disk;

    bool _disk_added;
};

// Device instance
static struct ramdisk *dev = NULL;

static int ramdisk_open(struct gendisk *disk, blk_mode_t mode)
{
    if(!blk_get_queue(disk->queue)){ 
        printk(KERN_ERR "ramdisk: blk_get_queue cannot get queue");
        return -ENXIO;
    }

    return 0;
}

static void ramdisk_release(struct gendisk *disk)
{
    blk_put_queue(disk->queue);
}

static int ramdisk_ioctl(struct block_device *bdev, fmode_t mode, unsigned cmd, unsigned long arg)
{
    if(cmd == HDIO_GETGEO) {
        printk(KERN_DEBUG "ramdisk: ramdisk_ioctl::HDIO_GETGEO");

        struct hd_geometry geo;
        geo.heads = 4;
        geo.sectors = 16;
        geo.cylinders = (dev->capacity_sectors & ~0x3f) >> 6;
        geo.start = 4;
        if (copy_to_user((void*)arg, &geo, sizeof(geo))) {
            printk(KERN_ERR "ramdisk: copy_to_user failed during HDIO_GETGEO");
            return -EFAULT;
        }
        return 0;
    }

    return -ENOTTY;
}

static struct block_device_operations ramdisk_ops = {
    .owner = THIS_MODULE,
    .open = ramdisk_open,
    .release = ramdisk_release,
    .ioctl = ramdisk_ioctl,
};

// Serve requests
static int ramdisk_process_request(struct request *rq, unsigned int *nr_bytes)
{
    int ret = 0;
    struct bio_vec bvec;
    struct req_iterator iter;
    struct ramdisk *dev = rq->q->queuedata;
    loff_t pos = blk_rq_pos(rq) << SECTOR_SHIFT;
    loff_t dev_size = (loff_t)(dev->capacity_sectors << SECTOR_SHIFT);

    // Iterate over all requests segments
    rq_for_each_segment(bvec, rq, iter)
    {
        unsigned long b_len = bvec.bv_len;

        // Get pointer to the data
        void* b_buf = page_address(bvec.bv_page) + bvec.bv_offset;

        // Simple check that we are not out of the memory bounds
        if ((pos + b_len) > dev_size) {
            b_len = (unsigned long)(dev_size - pos);
        }

        if (rq_data_dir(rq) == WRITE) {
            // Copy data to the buffer in to required position
            memcpy(dev->data + pos, b_buf, b_len);
        } else {
            // Read data from the buffer's position
            memcpy(b_buf, dev->data + pos, b_len);
        }

        // Increment counters
        pos += b_len;
        *nr_bytes += b_len;
    }

    return ret;
}

// queue callback function
static blk_status_t ramdisk_queue_request(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data* bd)
{
    unsigned int nr_bytes = 0;
    blk_status_t status = BLK_STS_OK;
    struct request *rq = bd->rq;

    // Start request serving procedure
    blk_mq_start_request(rq);

    if (ramdisk_process_request(rq, &nr_bytes) != 0) {
        status = BLK_STS_IOERR;
    }

    // Notify kernel about processed nr_bytes
    if (blk_update_request(rq, status, nr_bytes)) {
        printk(KERN_ERR "ramdisk: blk_update_request Failed");
        // Shouldn't fail
        BUG();
    }

    // Stop request serving procedure
    __blk_mq_end_request(rq, status);

    return status;
}

static struct blk_mq_ops ramdisk_mq_ops = {
    .queue_rq = ramdisk_queue_request,
};

static void ramdisk_cleanup(void) 
{
    if(dev) {
        if(dev->_disk_added) {
            // Remove the block device
            del_gendisk(dev->disk);
            // Free the block device 
            put_disk(dev->disk);
            // For belt and braces if there's a failure later on
            dev->_disk_added = false;
        }

        if(dev->tag_set) {
            blk_mq_free_tag_set(dev->tag_set);
            kfree(dev->tag_set);
        }

        if(dev->data) {
            vfree(dev->data);
        }

        kfree(dev);
        dev = NULL;
    }

    if(dev_major>0) {
        unregister_blkdev(dev_major, "ramdisk");
    }
}

static int __init ramdisk_driver_init(void)
{
    // Register new block device and get device major number
    dev_major = register_blkdev(dev_major, "ramdisk");
    if (dev_major < 0) {
        printk(KERN_ERR "ramdisk: register_blkdev failed\n");
        return -EBUSY;
    }

    // Allocate the block device structure
    dev = kzalloc(sizeof(struct ramdisk), GFP_KERNEL);
    dev->_disk_added = false;

    if (dev == NULL) {
        printk(KERN_ERR "ramdisk: Failed to allocate struct dev\n");
        ramdisk_cleanup();

        return -ENOMEM;
    }

    // Allocate the actual ramdisk from virtual memory
    dev->capacity_sectors = (TOTAL_SECTORS * SECTOR_SIZE) >> SECTOR_SHIFT;

    // Allocate corresponding data buffer
    dev->data = vmalloc(dev->capacity_sectors << SECTOR_SHIFT);
    if (dev->data == NULL) {
        printk(KERN_ERR "ramdisk: Failed to allocate device IO buffer\n");
        ramdisk_cleanup();

        return -ENOMEM;
    }

    // Allocate new disk
    dev->disk = blk_alloc_disk(1);
    if (dev->disk == NULL) {
        printk(KERN_ERR "ramdisk: blk_alloc_disk failed");
        ramdisk_cleanup();

        return -ENOMEM;
    }

    // Initialise and Configure the tag set for queue
    dev->tag_set = kzalloc(sizeof(struct blk_mq_tag_set), GFP_KERNEL);
    if (dev->tag_set == NULL) {
        printk(KERN_ERR "ramdisk: Failed to allocate blk_mq_tag_set\n");
        ramdisk_cleanup();

        return -ENOMEM;
    }
    dev->tag_set->ops = &ramdisk_mq_ops;
    dev->tag_set->queue_depth = 128;
    dev->tag_set->numa_node = NUMA_NO_NODE;
    dev->tag_set->flags = BLK_MQ_F_SHOULD_MERGE;
    dev->tag_set->nr_hw_queues = 1;
    dev->tag_set->cmd_size = 0;

    // Set it up in the system
    int err = blk_mq_alloc_tag_set(dev->tag_set);
    if (err) {
        printk(KERN_ERR "ramdisk: blk_mq_alloc_tag_set returned error %d\n", err);
        ramdisk_cleanup();

        return -ENOMEM;
    }

    // Allocate queues
    if (blk_mq_init_allocated_queue(dev->tag_set, dev->disk->queue)) {
        printk(KERN_ERR "ramdisk: blk_mq_init_allocated_queue failed");
        ramdisk_cleanup();

        return -ENOMEM;
    }
    blk_queue_rq_timeout(dev->disk->queue, BLK_DEFAULT_SG_TIMEOUT);
    dev->disk->queue->queuedata = dev;

    // Set all required flags and data
    dev->disk->flags = GENHD_FL_NO_PART;
    dev->disk->major = dev_major;
    dev->disk->first_minor = 0;
    dev->disk->minors = 1;
    dev->disk->fops = &ramdisk_ops;
    dev->disk->private_data = dev;

    // Set device name as it will be represented in /dev
    sprintf(dev->disk->disk_name, "ramdisk");

    // Set device capacity_sectors
    set_capacity(dev->disk, dev->capacity_sectors);

    // Set the logical block size
    blk_queue_logical_block_size(dev->disk->queue, SECTOR_SIZE);

    // Notify kernel about new disk device
    err = device_add_disk(NULL, dev->disk, NULL);
    if (err) {
        printk(KERN_ERR "ramdisk: device_add_disk returned error %d\n", err);
        ramdisk_cleanup();

        return -ENOMEM;
    }

    // Mark add_disk succeeded
    dev->_disk_added = true;

    return 0;
}

static void __exit ramdisk_driver_exit(void)
{
    ramdisk_cleanup();
}

module_init(ramdisk_driver_init);
module_exit(ramdisk_driver_exit);

MODULE_LICENSE("GPL");
