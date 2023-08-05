/* Prototype module for second mandatory DM510 assignment */
#ifndef __KERNEL__
#  define __KERNEL__
#endif
#ifndef MODULE
#  define MODULE
#endif

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>	
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/wait.h>
/* #include <asm/uaccess.h> */
#include <linux/uaccess.h>
#include <linux/semaphore.h>
/* #include <asm/system.h> */
#include <linux/cdev.h>
#include <asm/switch_to.h>
#include <asm/atomic.h>
#include <linux/ioctl.h>
/* Prototypes - this would normally go in a .h file */
static int dm510_open( struct inode*, struct file* );
static int dm510_release( struct inode*, struct file* );
static ssize_t dm510_read( struct file*, char*, size_t, loff_t* );
static ssize_t dm510_write( struct file*, const char*, size_t, loff_t* );
long dm510_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

#define DEVICE_NAME "dm510_dev" /* Dev name as it appears in /proc/devices */
#define DM510_IOC_MAGIC 'k'
#define DM510_IOCS_BUFFERSIZE _IOW(DM510_IOC_MAGIC, 1, int)
#define DM510_IOCG_BUFFERSIZE _IOR(DM510_IOC_MAGIC, 2, int)
#define DM510_IOCS_MAXREADERS _IOW(DM510_IOC_MAGIC, 3, int)
#define DM510_IOCG_MAXREADERS _IOR(DM510_IOC_MAGIC, 4, int)
#define DM510_IOC_MAXNR 4
#define DEVICE_COUNT 2
#define BUFFER_SZ	 40		// Default buffer size
#define MAX_READERS	 4		// Default max readers
/* end of what really should have been in a .h file */

/* file operations struct */
static struct file_operations dm510_fops = {
	.owner   = THIS_MODULE,
	.read    = dm510_read,
	.write   = dm510_write,
	.open    = dm510_open,
	.release = dm510_release,
        .unlocked_ioctl   = dm510_ioctl
};

int dm510_major = 255;
int dm510_minor = 0;
// ioctl params
static int g_buffersize = BUFFER_SZ;
static int g_max_readers = MAX_READERS;

static struct dm510_buffer {
	wait_queue_head_t readq, writeq;
	char *buffer, *end;
	int buffersize;
	char *rp, *wp;
	struct mutex mutex;
} dm510_buffer_arr[2];

static struct dm510_dev {
	struct dm510_buffer *read_buffer, *write_buffer;
	atomic_t write_avail, nreaders;
	struct cdev cdev;
} dm510_dev_arr[2] = {
	{
		.read_buffer = &dm510_buffer_arr[0],
		.write_buffer = &dm510_buffer_arr[1]
	},
	{
		.read_buffer = &dm510_buffer_arr[1],
		.write_buffer = &dm510_buffer_arr[0]
	}
};

void setup_cdev(struct cdev *cdev, int index)
{
	int err;
	dev_t devno = MKDEV(dm510_major, 0 + index);

	cdev_init(cdev, &dm510_fops);
	cdev->owner = THIS_MODULE;
	err = cdev_add(cdev, devno, 1);
	printk(KERN_DEBUG "DM510: loaded char dev %i at %x\n", index, devno);

	if (err){
		printk(KERN_NOTICE "Error %d adding dm510-%d", err, (int) index);
	}
}

/* called when module is loaded */
int dm510_init_module( void ) {
	int result, err;
	dev_t dev;
	
        dev = MKDEV(dm510_major,dm510_minor);
	
	// allocate chrdev
	result = register_chrdev_region(dev, DEVICE_COUNT, DEVICE_NAME);
	
	if (result < 0)
	{
		printk(KERN_WARNING "DM510: can't allocate chrdev\n");
		return result;
	}

	// initialize buffers
	for (size_t i = 0; i < 2; i++)
	{
		init_waitqueue_head(&dm510_buffer_arr[i].readq);
		init_waitqueue_head(&dm510_buffer_arr[i].writeq);
		mutex_init(&dm510_buffer_arr[i].mutex);
		// allocate buffer
		dm510_buffer_arr[i].buffer = kmalloc(g_buffersize, GFP_KERNEL);
		if(!dm510_buffer_arr[i].buffer)
		{
			printk(KERN_WARNING "DM510: Failed to allocate buffer %d\n", (int) i);
			err = -ENOMEM;
			goto fail;
		}
		printk(KERN_DEBUG "DM510: Allocated buffer at %p\n", dm510_buffer_arr[i].buffer);
		dm510_buffer_arr[i].buffersize = g_buffersize;
		dm510_buffer_arr[i].end = dm510_buffer_arr[i].buffer + dm510_buffer_arr[i].buffersize;
		dm510_buffer_arr[i].rp = dm510_buffer_arr[i].wp = dm510_buffer_arr[i].buffer;
	}

	// initialize devices
	for (size_t i = 0; i < DEVICE_COUNT; i++)
	{
		atomic_set(&dm510_dev_arr[i].write_avail, 1);
		atomic_set(&dm510_dev_arr[i].nreaders, 0);
		setup_cdev(&dm510_dev_arr[i].cdev, i);
	}
	
	printk(KERN_INFO "DM510: Module loaded\n");
	return 0;

	fail:
	for (size_t i = 0; i < DEVICE_COUNT; i++)
	{
		kfree(&dm510_buffer_arr[i].buffer);
		dm510_buffer_arr[i].buffer = NULL;
	}
	return err;
	
}

/* Called when module is unloaded */
void dm510_cleanup_module( void ) {
	dev_t devno = MKDEV(dm510_major, 0);

	for (size_t i = 0; i < DEVICE_COUNT; i++)
	{
		cdev_del(&dm510_dev_arr[i].cdev);
		kfree(dm510_buffer_arr[i].buffer);
	}
	unregister_chrdev_region(devno, DEVICE_COUNT);

	printk(KERN_INFO "DM510: Module unloaded.\n");
}


/* Called when a process tries to open the device file */
static int dm510_open( struct inode *inode, struct file *filp ) {
	struct dm510_dev *dev;
	dev = container_of(inode->i_cdev, struct dm510_dev, cdev);

	if (filp->f_mode & FMODE_WRITE)
	{
		// only allow one reader
		if (! atomic_dec_and_test(&dev->write_avail)) {
			atomic_inc(&dev->write_avail);
			return -EBUSY;
		}
		// open write
	}
	if (filp->f_mode & FMODE_READ)
	{
		// check for max readers
		if( atomic_inc_return(&dev->nreaders) > g_max_readers )
		{
			atomic_dec(&dev->nreaders);
			return -EBUSY;
		}
		// open read
	}
	filp->private_data = dev;
	
	return nonseekable_open(inode, filp);
}


/* Called when a process closes the device file. */
static int dm510_release( struct inode *inode, struct file *filp ) {
	struct dm510_dev *dev;
	dev = container_of(inode->i_cdev, struct dm510_dev, cdev);

	// release device
	if (filp->f_mode & FMODE_WRITE)
	{
		atomic_inc(&dev->write_avail);
	}
	else if (filp->f_mode & FMODE_READ)
	{
		atomic_dec(&dev->nreaders);
	}

	return 0;
}


/* Called when a process, which already opened the dev file, attempts to read from it. */
static ssize_t dm510_read( struct file *filp,
    char *buf,      /* The buffer to fill with data     */
    size_t count,   /* The max number of bytes to read  */
    loff_t *f_pos )  /* The offset in the file           */
{
	struct dm510_buffer *bbuf = ((struct dm510_dev *)filp->private_data)->read_buffer;


	if(mutex_lock_interruptible(&bbuf->mutex))
		return -ERESTARTSYS;

	while (bbuf->rp == bbuf->wp)	// nothing to read
	{
		mutex_unlock(&bbuf->mutex);
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;	// Immediately return if non-blocking
		// otherwise go to sleep
		printk(KERN_DEBUG "DM510: %i reading, going to sleep\n", current->pid);
		if(wait_event_interruptible(bbuf->readq, (bbuf->rp != bbuf->wp)))
			return -ERESTARTSYS;

		// reacquire mutex
		if (mutex_lock_interruptible(&bbuf->mutex))
			return -ERESTARTSYS;
	}

	// get data
	if (bbuf->wp > bbuf->rp)
		count = min(count, (size_t)(bbuf->wp - bbuf->rp));
	else	// write pointer wrapped
		count = min(count, (size_t)(bbuf->end - bbuf->rp));
	if (copy_to_user(buf, bbuf->rp, count)) {
		mutex_unlock(&bbuf->mutex);
		return -EFAULT;
	}
	bbuf->rp += count;
	if (bbuf->rp == bbuf->end)
		bbuf->rp = bbuf->buffer;	// wrap
	mutex_unlock(&bbuf->mutex);
	
	// wakeup writers
	wake_up_interruptible(&bbuf->writeq);
	printk(KERN_DEBUG "DM510: %i read %li bytes\n", current->pid, (long)count);
	return count; //return number of bytes read
}

int spacefree(struct dm510_buffer *bbuf)
{
	if (bbuf->rp == bbuf->wp)
		return bbuf->buffersize - 1;
	return ((bbuf->rp + bbuf->buffersize - bbuf->wp) % bbuf->buffersize) - 1;
}

/* Called when a process writes to dev file */
static ssize_t dm510_write( struct file *filp,
    const char *buf,/* The buffer to get data from      */
    size_t count,   /* The max number of bytes to write */
    loff_t *f_pos )  /* The offset in the file           */
{
	struct dm510_buffer *bbuf = ((struct dm510_dev *)filp->private_data)->write_buffer;

	if(mutex_lock_interruptible(&bbuf->mutex))
		return -ERESTARTSYS;

	// wait for space
	while(spacefree(bbuf) == 0) // full
	{
		mutex_unlock(&bbuf->mutex);
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		// otherwise go to sleep
		printk(KERN_DEBUG "DM510: %i writing, going to sleep\n", current->pid);
		if(wait_event_interruptible(bbuf->writeq, (spacefree(bbuf) != 0)))
			return -ERESTARTSYS;

		// reacquire mutex
		if (mutex_lock_interruptible(&bbuf->mutex))
			return -ERESTARTSYS;
	}

	// space available
	count = min(count, (size_t)spacefree(bbuf));
	if (bbuf->wp >= bbuf->rp)
		count = min(count, (size_t)(bbuf->end - bbuf->wp)); /* to end-of-buf */
	else /* the write pointer has wrapped, fill up to rp-1 */
		count = min(count, (size_t)(bbuf->rp - bbuf->wp - 1));
	printk(KERN_DEBUG "DM510: %i writing %li bytes\n", current->pid, (long)count);
	if (copy_from_user(bbuf->wp, buf, count)) {
		mutex_unlock(&bbuf->mutex);
		return -EFAULT;
	}
	bbuf->wp += count;
	if (bbuf->wp == bbuf->end)
		bbuf->wp = bbuf->buffer;	//wrap
	mutex_unlock(&bbuf->mutex);

	// awaken readers
	wake_up_interruptible(&bbuf->readq);
	
	return count;
}

/* called by system call icotl */ 
long dm510_ioctl( 
    struct file *filp, 
    unsigned int cmd,   /* command passed from the user */
    unsigned long arg ) /* argument of the command */
{
    int err = 0;
    int retval = 0;

    if (_IOC_TYPE(cmd) != DM510_IOC_MAGIC)
        return -ENOTTY;
    if (_IOC_NR(cmd) > DM510_IOC_MAXNR)
        return -ENOTTY;

    switch (cmd) {
        case DM510_IOCS_BUFFERSIZE:
            err = get_user(g_buffersize, (int __user *)arg);
            break;

        case DM510_IOCG_BUFFERSIZE:
            err = put_user(g_buffersize, (int __user *)arg);
            break;

        case DM510_IOCS_MAXREADERS:
            err = get_user(g_max_readers, (int __user *)arg);
            break;

        case DM510_IOCG_MAXREADERS:
            err = put_user(g_max_readers, (int __user *)arg);
            break;

        default:
            return -ENOTTY;
    }

    return retval;
}

module_init( dm510_init_module );
module_exit( dm510_cleanup_module );

MODULE_AUTHOR( "...EsraDuran" );
MODULE_LICENSE( "GPL" );
