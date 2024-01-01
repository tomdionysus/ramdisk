//
// Ramdisk device driver
//
// Original by Oleg Kutkov (https://olegkutkov.me/2020/02/10/linux-block-device-driver)
// Adapted for linux-6.6.7-0-lts by Tom Cully 2023
//
// Tested under linux 6.6.7-0-lts
// 

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

static int dev_major = 0;

struct ramdisk_device {
    sector_t capacity_sectors;
    u8 *data;   // Data buffer to emulate real storage device
    struct blk_mq_tag_set *tag_set;
    struct gendisk *disk;

    bool _disk_added;
};

// Device instance
static struct ramdisk_device *ramdisk_device = NULL;

static int blockdev_open(struct gendisk *disk, blk_mode_t mode)
{
    printk(KERN_DEBUG "ramdisk: blockdev_open");
    if(!blk_get_queue(disk->queue)){ 
        printk(KERN_ERR "ramdisk: blk_get_queue cannot get queue");
        return -ENXIO;
    }
    printk(KERN_DEBUG "ramdisk: blk_get_queue complete");

    return 0;
}

static void blockdev_release(struct gendisk *disk)
{
    printk(KERN_DEBUG "ramdisk: blockdev_release");
    blk_put_queue(disk->queue);
    printk(KERN_DEBUG "ramdisk: blk_put_queue complete");
}

static int blockdev_ioctl(struct block_device *bdev, fmode_t mode, unsigned cmd, unsigned long arg)
{
    printk(KERN_DEBUG "ramdisk: blockdev_ioctl command %d", cmd);

    struct hd_geometry geo;

    if(cmd == HDIO_GETGEO) {
        printk(KERN_DEBUG "ramdisk: blockdev_ioctl::HDIO_GETGEO");

        geo.heads = 4;
        geo.sectors = 16;
        geo.cylinders = (ramdisk_device->capacity_sectors & ~0x3f) >> 6;
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
    .open = blockdev_open,
    .release = blockdev_release,
    .ioctl = blockdev_ioctl,
};

// Serve requests
static int process_request(struct request *rq, unsigned int *nr_bytes)
{
    int ret = 0;
    struct bio_vec bvec;
    struct req_iterator iter;
    struct ramdisk_device *dev = rq->q->queuedata;
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
static blk_status_t queue_request(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data* bd)
{
    unsigned int nr_bytes = 0;
    blk_status_t status = BLK_STS_OK;
    struct request *rq = bd->rq;

    // Start request serving procedure
    blk_mq_start_request(rq);

    if (process_request(rq, &nr_bytes) != 0) {
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

static struct blk_mq_ops mq_ops = {
    .queue_rq = queue_request,
};

static void do_cleanup(void) {
    printk(KERN_DEBUG "ramdisk: do_cleanup\n");

    if(ramdisk_device) {
        if(ramdisk_device->_disk_added) {
            // Remove the block device
            del_gendisk(ramdisk_device->disk);
            // Free the block device 
            put_disk(ramdisk_device->disk);
            // For belt and braces if there's a failure later on
            ramdisk_device->_disk_added = false;
        }

        if(ramdisk_device->tag_set) {
            blk_mq_free_tag_set(ramdisk_device->tag_set);
            kfree(ramdisk_device->tag_set);
        }

        if(ramdisk_device->data) {
            vfree(ramdisk_device->data);
        }

        kfree(ramdisk_device);
        ramdisk_device = NULL;
    }

    if(dev_major>0) {
        unregister_blkdev(dev_major, "ramdisk");
    }
}

static int __init ramdisk_driver_init(void)
{
    int err;

    printk(KERN_DEBUG "ramdisk: ramdisk_driver_init");

    // Register new block device and get device major number
    dev_major = register_blkdev(dev_major, "ramdisk");
    if (dev_major < 0) {
        printk(KERN_ERR "ramdisk: register_blkdev failed\n");
        return -EBUSY;
    }
    printk(KERN_DEBUG "ramdisk: Got dev_major %d\n", dev_major);

    // Allocate the block device structure
    ramdisk_device = kzalloc(sizeof(struct ramdisk_device), GFP_KERNEL);
    ramdisk_device->_disk_added = false;

    if (ramdisk_device == NULL) {
        printk(KERN_ERR "ramdisk: Failed to allocate struct ramdisk_device\n");
        do_cleanup();

        return -ENOMEM;
    }

    // Allocate the actual ramdisk from virtual memory
    ramdisk_device->capacity_sectors = (TOTAL_SECTORS * SECTOR_SIZE) >> SECTOR_SHIFT;

    // Allocate corresponding data buffer
    ramdisk_device->data = vmalloc(ramdisk_device->capacity_sectors << SECTOR_SHIFT);
    if (ramdisk_device->data == NULL) {
        printk(KERN_ERR "ramdisk: Failed to allocate device IO buffer\n");
        do_cleanup();

        return -ENOMEM;
    }
    printk(KERN_DEBUG "ramdisk: Allocated %d bytes\n", TOTAL_SECTORS * SECTOR_SIZE);

    // Allocate new disk
    ramdisk_device->disk = blk_alloc_disk(1);
    if (ramdisk_device->disk == NULL) {
        printk(KERN_ERR "ramdisk: blk_alloc_disk failed");
        do_cleanup();

        return -ENOMEM;
    }
    printk(KERN_DEBUG "ramdisk: blk_alloc_disk complete\n");

    // Initialise and Configure the tag set for queue
    printk(KERN_DEBUG "ramdisk: Initializing tag_set\n");
    ramdisk_device->tag_set = kzalloc(sizeof(struct blk_mq_tag_set), GFP_KERNEL);
    if (ramdisk_device->tag_set == NULL) {
        printk(KERN_ERR "ramdisk: Failed to allocate blk_mq_tag_set\n");
        do_cleanup();

        return -ENOMEM;
    }
    ramdisk_device->tag_set->ops = &mq_ops;
    ramdisk_device->tag_set->queue_depth = 128;
    ramdisk_device->tag_set->numa_node = NUMA_NO_NODE;
    ramdisk_device->tag_set->flags = BLK_MQ_F_SHOULD_MERGE;
    ramdisk_device->tag_set->nr_hw_queues = 1;
    ramdisk_device->tag_set->cmd_size = 0;

    // Set it up in the system
    err = blk_mq_alloc_tag_set(ramdisk_device->tag_set);
    if (err) {
        printk(KERN_ERR "ramdisk: blk_mq_alloc_tag_set returned error %d\n", err);
        do_cleanup();

        return -ENOMEM;
    }
    printk(KERN_DEBUG "ramdisk: blk_mq_alloc_sq_tag_set complete\n");

    // Allocate queues
    printk(KERN_DEBUG "ramdisk: blk_mq_init_allocated_queue...\n");
    if (blk_mq_init_allocated_queue(ramdisk_device->tag_set, ramdisk_device->disk->queue)) {
        printk(KERN_ERR "ramdisk: blk_mq_init_allocated_queue failed");
        do_cleanup();

        return -ENOMEM;
    }
    blk_queue_rq_timeout(ramdisk_device->disk->queue, BLK_DEFAULT_SG_TIMEOUT);
    ramdisk_device->disk->queue->queuedata = ramdisk_device;
    printk(KERN_DEBUG "ramdisk: blk_mq_init_allocated_queue complete\n");

    // Set all required flags and data
    ramdisk_device->disk->flags = GENHD_FL_NO_PART;
    ramdisk_device->disk->major = dev_major;
    ramdisk_device->disk->first_minor = 0;
    ramdisk_device->disk->minors = 1;
    ramdisk_device->disk->fops = &ramdisk_ops;
    ramdisk_device->disk->private_data = ramdisk_device;

    // Set device name as it will be represented in /dev
    sprintf(ramdisk_device->disk->disk_name, "ramdisk");

    // Set device capacity_sectors
    set_capacity(ramdisk_device->disk, ramdisk_device->capacity_sectors);

    // Set the logical block size
    blk_queue_logical_block_size(ramdisk_device->disk->queue, SECTOR_SIZE);

    // Notify kernel about new disk device
    printk(KERN_DEBUG "ramdisk: Adding disk %s\n", ramdisk_device->disk->disk_name);
    err = device_add_disk(NULL, ramdisk_device->disk, NULL);
    if (err) {
        printk(KERN_ERR "ramdisk: device_add_disk returned error %d\n", err);
        do_cleanup();

        return -ENOMEM;
    }

    // Mark add_disk succeeded
    ramdisk_device->_disk_added = true;

    printk(KERN_DEBUG "ramdisk: Init Complete");

    return 0;
}

static void __exit ramdisk_driver_exit(void)
{
    printk(KERN_DEBUG "ramdisk: ramdisk_driver_exit");
    do_cleanup();
    printk(KERN_DEBUG "ramdisk: ramdisk_driver_exit Complete");
}

module_init(ramdisk_driver_init);
module_exit(ramdisk_driver_exit);

MODULE_LICENSE("GPL");
