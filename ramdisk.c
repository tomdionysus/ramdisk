//
// Ramdisk device driver
//
// Original by Oleg Kutkov (https://olegkutkov.me/2020/02/10/linux-block-device-driver)
// Adapted for linux-6.6.7-0-lts by Tom Cully 2023
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/blk-mq.h>
#include <linux/hdreg.h>

#ifndef SECTOR_SIZE
#define SECTOR_SIZE 512
#endif

#define TOTAL_PAGES 128

static int dev_major = 0;

/* Just internal representation of the our block device
 * can hold any useful data */
struct block_dev {
    sector_t capacity;
    u8 *data;   /* Data buffer to emulate real storage device */
    struct blk_mq_tag_set *tag_set;
    struct request_queue *queue;
    struct gendisk *gdisk;
};

/* Device instance */
static struct block_dev *block_device = NULL;

static int blockdev_open(struct gendisk *disk, blk_mode_t mode)
{
    printk(KERN_INFO "ramdisk: >>> blockdev_open\n");

    return 0;
}

static void blockdev_release(struct gendisk *disk)
{
    printk(KERN_INFO "ramdisk: >>> blockdev_release\n");
}

int blockdev_ioctl(struct block_device *bdev, fmode_t mode, unsigned cmd, unsigned long arg)
{
    printk(KERN_INFO "ramdisk: ioctl cmd 0x%08x\n", cmd);

    return -ENOTTY;
}

/* Set block device file I/O */
static struct block_device_operations blockdev_ops = {
    .owner = THIS_MODULE,
    .open = blockdev_open,
    .release = blockdev_release,
    .ioctl = blockdev_ioctl
};

/* Serve requests */
static int do_request(struct request *rq, unsigned int *nr_bytes)
{
    int ret = 0;
    struct bio_vec bvec;
    struct req_iterator iter;
    struct block_dev *dev = rq->q->queuedata;
    loff_t pos = blk_rq_pos(rq) << SECTOR_SHIFT;
    loff_t dev_size = (loff_t)(dev->capacity << SECTOR_SHIFT);

    printk(KERN_INFO KERN_WARNING "sblkdev: request start from sector %lld  pos = %lld  dev_size = %lld\n", blk_rq_pos(rq), pos, dev_size);

    /* Iterate over all requests segments */
    rq_for_each_segment(bvec, rq, iter)
    {
        unsigned long b_len = bvec.bv_len;

        /* Get pointer to the data */
        void* b_buf = page_address(bvec.bv_page) + bvec.bv_offset;

        /* Simple check that we are not out of the memory bounds */
        if ((pos + b_len) > dev_size) {
            b_len = (unsigned long)(dev_size - pos);
        }

        if (rq_data_dir(rq) == WRITE) {
            /* Copy data to the buffer in to required position */
            memcpy(dev->data + pos, b_buf, b_len);
        } else {
            /* Read data from the buffer's position */
            memcpy(b_buf, dev->data + pos, b_len);
        }

        /* Increment counters */
        pos += b_len;
        *nr_bytes += b_len;
    }

    return ret;
}

/* queue callback function */
static blk_status_t queue_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data* bd)
{
    printk(KERN_DEBUG "ramdisk: queue_rq Called");

    unsigned int nr_bytes = 0;
    blk_status_t status = BLK_STS_OK;
    struct request *rq = bd->rq;

    /* Start request serving procedure */
    blk_mq_start_request(rq);

    if (do_request(rq, &nr_bytes) != 0) {
        status = BLK_STS_IOERR;
    }

    /* Notify kernel about processed nr_bytes */
    if (blk_update_request(rq, status, nr_bytes)) {
        /* Shouldn't fail */
        BUG();
    }

    /* Stop request serving procedure */
    __blk_mq_end_request(rq, status);

    return status;
}

static struct blk_mq_ops mq_ops = {
    .queue_rq = queue_rq,
};

static int __init ramdisk_driver_init(void)
{
    printk(KERN_DEBUG "ramdisk: Initialising ramdisk driver");

    /* Register new block device and get device major number */
    dev_major = register_blkdev(dev_major, "ramdisk");

    if (dev_major < 0) {
             printk(KERN_ERR "ramdisk: register_blkdev failed\n");
             return -EBUSY;
     }

    printk(KERN_DEBUG "ramdisk: Got dev_major %d\n", dev_major);

    block_device = kmalloc(sizeof(struct block_dev), GFP_KERNEL);

    if (block_device == NULL) {
        printk(KERN_ERR "ramdisk: Failed to allocate struct block_dev\n");
        unregister_blkdev(dev_major, "ramdisk");

        return -ENOMEM;
    }

    /* Set some random capacity of the device */
    block_device->capacity = (TOTAL_PAGES * PAGE_SIZE) >> 9; /* nsectors * SECTOR_SIZE; */
    /* Allocate corresponding data buffer */
    block_device->data = kmalloc(block_device->capacity << 9, GFP_KERNEL);

    printk(KERN_DEBUG "ramdisk: Allocated %d bytes\n", ksize(block_device->data));
    
    if (block_device->data == NULL) {
        printk(KERN_ERR "ramdisk: Failed to allocate device IO buffer\n");
        unregister_blkdev(dev_major, "ramdisk");
        kfree(block_device);

        return -ENOMEM;
    }

    printk(KERN_DEBUG "ramdisk: Initializing queue\n");

    block_device->tag_set = kmalloc(sizeof(struct blk_mq_tag_set), GFP_KERNEL);
    
    if (block_device->tag_set == NULL) {
        printk(KERN_ERR "ramdisk: Failed to allocate blk_mq_tag_set\n");
        unregister_blkdev(dev_major, "ramdisk");
        kfree(block_device);

        return -ENOMEM;
    }

    blk_mq_alloc_sq_tag_set(block_device->tag_set, &mq_ops, 128, BLK_MQ_F_SHOULD_MERGE);

    printk(KERN_DEBUG "ramdisk: blk_mq_alloc_sq_tag_set complete\n");

    block_device->queue = blk_mq_init_queue(block_device->tag_set);

    if (block_device->queue == NULL) {
        printk(KERN_ERR "ramdisk: Failed to allocate device queue\n");
        kfree(block_device->data);

        unregister_blkdev(dev_major, "ramdisk");
        kfree(block_device);

        return -ENOMEM;
    }

    printk(KERN_DEBUG "ramdisk: blk_mq_init_queue complete\n");

    /* Set driver's structure as user data of the queue */
    block_device->queue->queuedata = block_device;

    /* Allocate new disk */
    block_device->gdisk = blk_alloc_disk(1);

    printk(KERN_DEBUG "ramdisk: blk_alloc_disk complete\n");

    /* Set all required flags and data */
    block_device->gdisk->flags = GENHD_FL_NO_PART;
    block_device->gdisk->major = dev_major;
    block_device->gdisk->first_minor = 0;

    block_device->gdisk->fops = &blockdev_ops;
    block_device->gdisk->queue = block_device->queue;
    block_device->gdisk->private_data = block_device;

    /* Set device name as it will be represented in /dev */
    strncpy(block_device->gdisk->disk_name, "ramdisk\0", 8);

    printk(KERN_DEBUG "ramdisk: Adding disk %s\n", block_device->gdisk->disk_name);

    /* Set device capacity */
    set_capacity(block_device->gdisk, block_device->capacity);
    
    printk(KERN_DEBUG "ramdisk: set_capacity complete\n");

    blk_queue_logical_block_size(block_device->gdisk->queue, SECTOR_SIZE);

    printk(KERN_DEBUG "ramdisk: blk_queue_logical_block_size complete\n");

    /* Notify kernel about new disk device */
    add_disk(block_device->gdisk);

    printk(KERN_DEBUG "ramdisk: Init Complete");

    return 0;
}

static void __exit ramdisk_driver_exit(void)
{
    /* Don't forget to cleanup everything */
    if (block_device->gdisk) {
        del_gendisk(block_device->gdisk);
        put_disk(block_device->gdisk);
    }

    if (block_device->queue) {
        blk_mq_destroy_queue(block_device->queue);
    }

    if (block_device->tag_set) {
        blk_mq_free_tag_set(block_device->tag_set);
        kfree(block_device->tag_set);
    }

    unregister_blkdev(dev_major, "ramdisk");

    kfree(block_device->data);
    kfree(block_device);
}

module_init(ramdisk_driver_init);
module_exit(ramdisk_driver_exit);

MODULE_LICENSE("GPL");
