#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>
#include <linux/mutex.h>

typedef unsigned int uint32;

//funtion declaration
static int open_pipe(struct inode *inode, struct file *filp);
static int close_pipe(struct inode *inode, struct file *filp);
static ssize_t send(struct file *filp, const char *buf, size_t count, loff_t *pos);
static ssize_t recv(struct file *filp, char *buf, size_t count, loff_t *pos);
static int pipe_init(void);
static void pipe_exit(void);

static struct file_operations pipe_fops = {
	.owner		=	THIS_MODULE,
	.open		=	open_pipe,
	.release	=   close_pipe,  
	.write		=   send,
	.read		=   recv,
};

module_init(pipe_init);
module_exit(pipe_exit);


//global var
int MAJOR_NUMBER = 185;
char DEVICE_NAME[] = "MyPipe";
int BUFFER_SIZE = 1024;

typedef enum PIPE_AUTHORITY {
	READONLY	= 0,
	WRITEONLY	= 1,
	READWRITE	= 2,
} pipe_auth;

//pipe device struct
typedef struct MYPIPE {
	char* buffer;
	pipe_auth auth;
	dev_t dev_number;
	struct cdev cdev;
} Mypipe;

//global pipe struct(for storing pipe info)
typedef struct GLOBAL_PIPE {
	char* buffer;
	size_t buf_size;	//buffer size
	size_t len;			//used size 
	size_t pos;			//current reading position
	struct mutex lock;  //lock for read/write
} Gpipe;

Gpipe gpipe;
Mypipe pipes[2];				//two devices


//initial driver
static int pipe_init(void) {
	int result, i;
	dev_t devno = MKDEV(MAJOR_NUMBER, 0);

	result = register_chrdev_region(devno, 2, DEVICE_NAME);
	//alloc failed
    if (result) {
		printk(KERN_ERR ": Mypipe: cannot allocate device number, error code %d\n", result);
		return result;
	}
	printk("Mypipe: alloc device number complete, major number: %d\n", MAJOR(devno));
	pipes[0].dev_number = MKDEV(MAJOR(devno), MINOR(devno));
	pipes[1].dev_number = MKDEV(MAJOR(devno), MINOR(devno) + 1);


	//register devices
	for(i = 0; i < 2; i++) {
		cdev_init(&(pipes[i].cdev), &pipe_fops);
		pipes[i].cdev.ops = &pipe_fops;

		result = cdev_add(&(pipes[i].cdev), pipes[i].dev_number, 1);
		//cdev_add failed
		if(result) {
			printk(KERN_ERR ": Mypipe: cannot register device, error code %d\n", result);
			if(i == 1) cdev_del(&(pipes[0].cdev));
			unregister_chrdev_region(pipes[0].dev_number, 2);
			return result;
		}
		printk("Mypipe: register pipe device %d complete, minor number: %d\n", i, MINOR(pipes[i].dev_number));
	}


	//initial global pipe struct 
	gpipe.buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);

	if (!(gpipe.buffer)) {
		printk("Mypipe: cannot allocate memory for global pipe buffer\n");
		result = -ENOMEM;
		pipe_exit();
		return result;
	}
	printk("Mypipe: allocate memory for global pipe buffer complete\n");

	memset(gpipe.buffer, 0, BUFFER_SIZE);

	gpipe.buf_size = BUFFER_SIZE;
	gpipe.len = 0;
	gpipe.pos = 0;
	mutex_init(&(gpipe.lock));

	printk("Mypipe: Inserting pipe module complete\n"); 

	return 0;
}


//remove driver
static void pipe_exit(void) {
	//unregister devices 
	int i;
	for(i = 0; i < 2; i++) {
		cdev_del(&(pipes[i].cdev));
		printk("Mypipe: delete pipe device %d complete, minor number: %d\n", i, MINOR(pipes[i].dev_number));
	}

	/* Freeing the major number */
	unregister_chrdev_region(pipes[0].dev_number, 2);

	/* Freeing global buffer memory */
	if (gpipe.buffer) kfree(gpipe.buffer);
	mutex_destroy(&(gpipe.lock));

	printk("Mypipe: Removing pipe module complete\n");
}


//open device 
static int open_pipe(struct inode *inode, struct file *filp) {
	Mypipe *dev;

	/*minor device*/
	int minor = MINOR(inode->i_rdev);

	//printk("Mypipe: Open pipe device complete\n");
	//if the device has been opened
	if(filp->private_data) {
		dev = filp->private_data; 	
	}
	else {
		if(minor >= 2) {
			printk("MyPipe: No such device %d\n", minor);
			return -ENODEV;
		}
	
		//readable pipe
		if(minor == 0) {
			dev = &(pipes[0]);
			dev->buffer = gpipe.buffer;
			dev->auth = READONLY;
		}
		//writable pipe
		else {
			dev = &(pipes[1]);
			dev->buffer = gpipe.buffer;
			dev->auth = WRITEONLY;
		}

		filp->private_data = dev;
	}
	
	try_module_get(THIS_MODULE);
	printk("Mypipe: Open pipe device complete\n");
	return 0;
}

//close device
static int close_pipe(struct inode *inode, struct file *filp) {
	Mypipe *dev = filp->private_data;
	
	dev->buffer = NULL;
	dev->auth = READONLY;
	filp->private_data = NULL;

	module_put(THIS_MODULE);
	printk("Mypipe: Close pipe device complete\n");
	return 0;
}


//send data 
static ssize_t send(struct file *filp, const char *buf, size_t count, loff_t *pos) {
	size_t i = 0;
	size_t wrt_pos = 0;
	Mypipe *dev = filp->private_data;

	printk("Mypipe: try to send\n");

	if(dev == NULL) {
		printk("Mypipe: device hasn't been open yet\n");
		return -1;
	}
	if(dev->auth != WRITEONLY) return -EPERM;

	while (i < count) {
		//buffer is full, return
		if(gpipe.len >= gpipe.buf_size) return -EAGAIN;

		//lock critical region
		mutex_lock(&(gpipe.lock));

		//round-robin char queue
		wrt_pos = (gpipe.pos + gpipe.len) % gpipe.buf_size;
		copy_from_user((dev->buffer + wrt_pos), (buf + i), 1);
		i += 1;
		gpipe.len += 1;

		mutex_unlock(&(gpipe.lock));
	}

	printk("Mypipe: send complete\n");

	return count;
}

//recieve data 
static ssize_t recv(struct file *filp, char *buf, size_t count, loff_t *pos) {
	size_t i = 0;
	Mypipe *dev = filp->private_data;

	printk("Mypipe: try to recv\n");

	if(dev == NULL) {
		printk("Mypipe: device hasn't been open yet\n");
		return -1;
	}
	if(dev->auth != READONLY) return -EPERM;

	while (i < count) {
		//buffer is empty, return all bytes
		if(gpipe.len == 0) {
			printk("Mypipe: recv partially complete\n");
			return i;
		}

		//lock critical region
		mutex_lock(&(gpipe.lock));
	
		//round-robin char queue
		copy_to_user((buf + i), (dev->buffer + gpipe.pos), 1);
		gpipe.pos = (gpipe.pos + 1) % gpipe.buf_size;
		i += 1;
		gpipe.len -= 1;

		mutex_unlock(&(gpipe.lock));
	}

	printk("Mypipe: recv complete\n");

	return count;
}

