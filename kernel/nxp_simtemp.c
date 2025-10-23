/*****************************************************************************
*  file              simptemp.c
*
*  description       Linux device driver for simulated temperature sensor
*
*  author            Elyoenai Martínez
*
*  kernel version    6.12.47+rpt-rpi-v8
*
*****************************************************************************/


/****************************************************************************
 * Includes
 ****************************************************************************/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>                 
#include <linux/uaccess.h>            
#include <linux/err.h>
#include <linux/proc_fs.h>
#include <linux/jiffies.h>
#include <linux/wait.h>                 
#include <linux/poll.h>
#include <linux/ktime.h>
#include <linux/kthread.h>
#include <linux/i2c.h>
#include <linux/timekeeping.h>
#include <linux/delay.h> 
#include <linux/rtc.h> 
#include "nxp_simtemp.h"

/****************************************************************************
 * Definitions
 ****************************************************************************/
#define SIMTEMP_DEV     "simtemp"
#define SIMTEMP_CLASS   "simtemp_class"
#define TIMEOUT 	    100

/****************************************************************************
 * Globals
 ***************************************************************************/
static dev_t simtemp;
static struct cdev simtemp_cdev;
static struct class *simtemp_class;
static struct device *simtemp_sysdev;
static struct task_struct *simtemp_thread1;
static struct task_struct *simtemp_thread2;
static int sysfs_sampling_ms;
static int sysfs_htemp_mC;
static int sysfs_ltemp_mC;
static char sysfs_mode[16];
static char sysfs_stats[128];
static unsigned short state = 0;
static int sampling_ms=1000;
static int ltemp_alert=5000;
static int htemp_alert=50000;
static bool timeout_flag = false;
static bool alert_flag = false;
static bool alert_on = false;
struct mutex simtemp_mutex;
#ifdef SIM
static struct timer_list simtemp_timer;
static unsigned int count = 0;
uint32_t random_value1, random_value2;
static int sim_temp;
#else
static struct i2c_client *simtemp_client;
#endif	

struct stats{
	uint64_t last_error_ns;
	unsigned short LOW_TEMP_ALERT   :1;
    unsigned short HIGH_TEMP_ALERT  :1;
    unsigned short                  :14;  
};

struct stats *stats_storage;

/****************************************************************************
 * Waitqueues declaration
 ****************************************************************************/
DECLARE_WAIT_QUEUE_HEAD(simtemp_wq_poll);
DECLARE_WAIT_QUEUE_HEAD(simtemp_wq_tout);

/****************************************************************************
 * Prototypes
 ****************************************************************************/
static int __init simtemp_init(void);
static void __exit simtemp_exit(void);
static int f_ops_open(struct inode *inode, struct file *file);
static int f_ops_release(struct inode *inode, struct file *file);
static ssize_t f_ops_read(struct file *filp, char *buf, size_t len, loff_t *offset);
static ssize_t f_ops_write(struct file *filp, const char *buf, size_t len, loff_t *offset);
static ssize_t sysfs_sampling_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t sysfs_sampling_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t sysfs_htemp_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t sysfs_htemp_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t sysfs_ltemp_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t sysfs_ltemp_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t sysfs_mode_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t sysfs_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t sysfs_stats_show(struct device *dev, struct device_attribute *attr, char *buf);
int thread_function_states(void *pv);
int thread_function_temp_meas(void *pv);
static unsigned int simtemp_poll(struct file *filp, struct poll_table_struct *wait);
static void measure_and_compare(simtemp_sample *ps);
#ifdef SIM
void timer_callback(struct timer_list *data);
#else
static int simtemp_probe(struct i2c_client *client);
static void simtemp_remove(struct i2c_client *client); 
#endif

/****************************************************************************
 * Struct for file operations
 ****************************************************************************/
static struct file_operations f_ops={
	.read    = f_ops_read,
	.write   = f_ops_write,
	.open    = f_ops_open,
	.release = f_ops_release,
	.poll    = simtemp_poll
};

/****************************************************************************
 * Bind device tree overlay
 ****************************************************************************/
 #ifndef SIM
static struct of_device_id simtemp_ids[] = {
	{
		.compatible = "nxp,simtemp",
	}, 
	{ }
};

MODULE_DEVICE_TABLE(of, simtemp_ids);

static struct i2c_device_id simtemp_dev_id[] = {
	{"simtemp", 0},
	{ },
};

MODULE_DEVICE_TABLE(i2c, simtemp_dev_id);

static struct i2c_driver simtemp_driver = {
	.probe = simtemp_probe,
	.remove = simtemp_remove,
	.id_table = simtemp_dev_id,
	.driver = {
		.name = "simtemp",
		.of_match_table = simtemp_ids,
	},
};
#endif

/****************************************************************************
 * Attributes for sysfs class in simtemp
 ***************************************************************************/
 DEVICE_ATTR(sysfs_sampling_ms, 0660, sysfs_sampling_show, sysfs_sampling_store);
 DEVICE_ATTR(sysfs_htemp_mC, 0660, sysfs_htemp_show, sysfs_htemp_store);
 DEVICE_ATTR(sysfs_ltemp_mC, 0660, sysfs_ltemp_show, sysfs_ltemp_store);
 DEVICE_ATTR(sysfs_mode, 0660, sysfs_mode_show, sysfs_mode_store);
 DEVICE_ATTR(sysfs_stats, 0660, sysfs_stats_show, NULL);
 
 static struct attribute *simtemp_attrs[] = {
        &dev_attr_sysfs_sampling_ms.attr,
        &dev_attr_sysfs_htemp_mC.attr,
        &dev_attr_sysfs_ltemp_mC.attr,
        &dev_attr_sysfs_mode.attr,
        &dev_attr_sysfs_stats.attr,
        NULL, 
};

static const struct attribute_group simtemp_group = {
        .attrs = simtemp_attrs,
};

static const struct attribute_group *simtemp_groups[] = {
	&simtemp_group,
	NULL,
};
 
/****************************************************************************
 * Timer callback function
 ****************************************************************************/
 #ifdef SIM
void timer_callback(struct timer_list *data)
{
	if(count == 0){
		get_random_bytes(&random_value1, sizeof(uint32_t));
		sim_temp = random_value1%50;
		sim_temp*=1000;
	}
	else if(count>0 && count <50)
	{
		sim_temp+=100;
	}
	else if(count == 50)
	{
		get_random_bytes(&random_value2, sizeof(uint32_t));
		sim_temp -= (random_value2%10)*100; 
	}
	else if(count>50 && count <100)
	{
		sim_temp-=150;
	}

	count++;
	if(count>=100)
		count = 0;
	
	mod_timer(&simtemp_timer, jiffies + msecs_to_jiffies(TIMEOUT));
}
#endif
 
/****************************************************************************
 * sysfs show functions
 ***************************************************************************/
/*Return value stored in sysfs attributes*/
static ssize_t sysfs_sampling_show(struct device *dev, struct device_attribute *attr, char *buf)
{

	return sprintf(buf, "%d\n", sysfs_sampling_ms);
}

static ssize_t sysfs_htemp_show(struct device *dev, struct device_attribute *attr, char *buf){
	return sprintf(buf, "%d\n", sysfs_htemp_mC);
}

static ssize_t sysfs_ltemp_show(struct device *dev, struct device_attribute *attr, char *buf){
	return sprintf(buf, "%d\n", sysfs_ltemp_mC);
}

static ssize_t sysfs_mode_show(struct device *dev, struct device_attribute *attr, char *buf){
	strcpy(buf, sysfs_mode);
	return strlen(sysfs_mode);
}

static ssize_t sysfs_stats_show(struct device *dev, struct device_attribute *attr, char *buf){
	struct rtc_time tm;
	tm = rtc_ktime_to_tm(stats_storage->last_error_ns);
	char error_date[64];
	snprintf(error_date, sizeof(error_date), "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	snprintf(sysfs_stats, sizeof(sysfs_stats), "Last error: %s GMT - Type of error: %s\n", error_date, (stats_storage->LOW_TEMP_ALERT == 1 ? "Low temperature" : "High temperature"));
	strcpy(buf,sysfs_stats);
	return strlen(sysfs_stats);
}

/****************************************************************************
 * sysfs store functions
 ***************************************************************************/
 /*Store values in sysfs attributes*/
static ssize_t sysfs_sampling_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int uspace_sample;
	if(kstrtoint(buf, 10, &uspace_sample) == 0){
		sysfs_sampling_ms = uspace_sample;
		sampling_ms = sysfs_sampling_ms;
	}
	state = 1;   
	wake_up(&simtemp_wq_tout);
	
	return count;
}

static ssize_t sysfs_htemp_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int uspace_htemp;
	if(kstrtoint(buf, 10, &uspace_htemp) == 0){
		sysfs_htemp_mC = uspace_htemp;
		htemp_alert = sysfs_htemp_mC;
	}
	state = 1;   
	wake_up(&simtemp_wq_tout);
	
	return count;
}

static ssize_t sysfs_ltemp_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int uspace_ltemp;
	if(kstrtoint(buf, 10, &uspace_ltemp) == 0){
		sysfs_ltemp_mC = uspace_ltemp;
		ltemp_alert = sysfs_ltemp_mC;
	}
	state = 1;   
	wake_up(&simtemp_wq_tout);
	
	return count;
}

static ssize_t sysfs_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{	
	strncpy(sysfs_mode, buf, sizeof(sysfs_mode));
	
	return count;
}


/****************************************************************************
 * thread functions
 ****************************************************************************/
int thread_function_states(void *pv)
{
	while(1){
			/*Wait for a change in the state variable or timeout to read temperature value*/
			wait_event_timeout(simtemp_wq_tout, state != 0, msecs_to_jiffies(sampling_ms));
			{
				if(state==0){
					wake_up(&simtemp_wq_poll);
					timeout_flag = true; 
					alert_on = false;
				}
			}
			if(state == 2){
				return 0;
			}
			state = 0;
	}
	return 0;	
}


int thread_function_temp_meas(void *pv){
	simtemp_sample simtemp_st; 

	
	while(!kthread_should_stop()){
#ifndef SIM		
			msleep(150);
#else			
			msleep(10);
#endif
			
			mutex_lock(&simtemp_mutex);	
			 			 
			measure_and_compare(&simtemp_st);
			 			 	
			mutex_unlock(&simtemp_mutex);
	
			/*Activate alert for low or high temperature*/
			if((simtemp_st.LOW_TEMP_ALERT == 1 || simtemp_st.HIGH_TEMP_ALERT == 1) && alert_on == false)
			 {
                alert_on = true;			 
				alert_flag = true;
				wake_up(&simtemp_wq_poll);	 						
			 }
			
	}
	return 0;
}

/****************************************************************************
 * Poll function
 ****************************************************************************/
static unsigned int simtemp_poll(struct file *filp, struct poll_table_struct *wait)
{
	poll_wait(filp, &simtemp_wq_poll, wait);
		
	if(timeout_flag){
		timeout_flag = false;
		return POLLIN | POLLRDNORM;	
	}
	
	if(alert_flag){
		alert_flag = false;
		return POLLIN | POLLRDNORM;	
	}
	
    return 0; 
}


/****************************************************************************
 * read temperature from device and compare limits
 ****************************************************************************/
static void measure_and_compare(simtemp_sample *simtemp_s){
#ifndef SIM
	int temp;
#endif
	int temp_mC;
	ktime_t current_time;
	
	/*Gets current time*/
	current_time = ktime_get_real_ns();
	simtemp_s->timestamp_ns = current_time;

#ifndef SIM	
	/*Get the temperature from the sensor*/
	temp = i2c_smbus_read_byte(simtemp_client);
	temp_mC = temp*1000;
#else
	/*Get simulated temperature from timer*/	
	temp_mC = sim_temp;
#endif	
	
	simtemp_s->temp_mC = temp_mC;
	
	/*Compare limits*/
	if(temp_mC <=ltemp_alert){
		simtemp_s->LOW_TEMP_ALERT = 1;		
		stats_storage->last_error_ns=current_time;
		stats_storage->LOW_TEMP_ALERT = 1;
		stats_storage->HIGH_TEMP_ALERT = 0;
	}
	else
		simtemp_s->LOW_TEMP_ALERT = 0;
					
	if(temp_mC >=htemp_alert){
		simtemp_s->HIGH_TEMP_ALERT = 1;		
		stats_storage->last_error_ns=current_time; 
		stats_storage->LOW_TEMP_ALERT = 0;
		stats_storage->HIGH_TEMP_ALERT = 1;
	}
	else
		simtemp_s->HIGH_TEMP_ALERT = 0;
}

/****************************************************************************
 * File operations - open function
 ****************************************************************************/
static int f_ops_open(struct inode *inode, struct file *file)
{
	return 0;
}

/****************************************************************************
 * File operations - release function
 ****************************************************************************/
static int f_ops_release(struct inode *inode, struct file *file)
{
	return 0;
}

/****************************************************************************
 * File operations - read function
 ****************************************************************************/
static ssize_t f_ops_read(struct file *filp, char *buf, size_t len, loff_t *offset){
	simtemp_sample simtemp_st; 
		
	mutex_lock(&simtemp_mutex);	

	measure_and_compare(&simtemp_st);

	mutex_unlock(&simtemp_mutex);	

	if(copy_to_user(buf, &simtemp_st, sizeof(simtemp_st))){
		printk(KERN_ERR "Error copying struct to userspace\n");
	}
			
    return 0;
}

/****************************************************************************
 * File operations - write function
 ****************************************************************************/
static ssize_t f_ops_write(struct file *filp, const char *buf, size_t len, loff_t *offset)
{
#ifdef SIM	
	/*Setting timer for the first-time run*/
	mod_timer(&simtemp_timer, jiffies + msecs_to_jiffies(TIMEOUT));
#endif
	wake_up_process(simtemp_thread1);
	wake_up_process(simtemp_thread2);
	return 0;
}


/****************************************************************************
 * Probe function
 ****************************************************************************/
#ifndef SIM 
static int simtemp_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	int dt_value=0;
	int ret_value=0;
	
	/*Get values from Device Tree*/
	ret_value = of_property_read_s32(dev->of_node, "ltemp_alert_mC", &dt_value);
	ltemp_alert = dt_value;
		
	ret_value = of_property_read_u32(dev->of_node, "htemp_alert_mC", &dt_value);
	htemp_alert = dt_value;
		
	ret_value = of_property_read_u32(dev->of_node, "sampling_ms", &dt_value);
	sampling_ms = dt_value;
				
	simtemp_client = client;
		
	return 0;
}
#endif
/****************************************************************************
 * Remove function
 ****************************************************************************/
#ifndef SIM  
static void simtemp_remove(struct i2c_client *client)
{
}
#endif


/****************************************************************************
 * Init function of the module
 ****************************************************************************/
static int __init simtemp_init(void)
{
	/*Dynamic allocation of major and minor numbers for character device*/
	if((alloc_chrdev_region(&simtemp, 0, 1, SIMTEMP_DEV))<0){
		printk(KERN_ERR "It is not possible to allocate major and minor numbers\n");
		return -1;
	}
	
	/*Initialize cdev structure and associate file operations*/
	cdev_init(&simtemp_cdev, &f_ops);
	
	/*Add character device*/
	if(cdev_add(&simtemp_cdev,simtemp,1)<0){
		printk(KERN_ERR "It is not possible to add the character device to the system\n");
		goto rem_cdev;
	}
    
    /*Create sysfs class*/
	if(IS_ERR(simtemp_class = class_create(SIMTEMP_CLASS))){
		printk(KERN_ERR "It is not possible to create the struct class\n");
		goto rem_cdev;	
	}

	/*Create device driver*/
	simtemp_sysdev = device_create_with_groups(simtemp_class,NULL,simtemp,NULL, simtemp_groups, SIMTEMP_DEV);
	if(IS_ERR(simtemp_sysdev)){
		printk(KERN_ERR "It is not possible to create the device driver\n");
		goto rem_class;	
	}

#ifndef SIM	
	/*Register i2c driver*/
	if(i2c_add_driver(&simtemp_driver)){
	  	printk(KERN_ERR "It is not possible to add the i2c driver\n");
	  	goto rem_device;
	}
#endif	
	
	/*Mutex initialization*/
	mutex_init(&simtemp_mutex);
	
	/*Create thread 1*/
	simtemp_thread1 = kthread_create(thread_function_states,NULL,"simtemp_thread1");
	if(!simtemp_thread1){
		printk(KERN_ERR "Error creating the thread 1\n");
		goto rem_device;
	}
	
	/*Create thread 2*/
	simtemp_thread2 = kthread_create(thread_function_temp_meas,NULL,"simtemp_thread2");
	if(!simtemp_thread2){
		printk(KERN_ERR "Error creating the thread 2\n");
		goto rem_device;
	}
	
#ifdef SIM	
	/*Setting up the timer*/
	timer_setup(&simtemp_timer, timer_callback, 0);
#endif

	/*Memory allocation for stats struct*/
	if((stats_storage = (struct stats*)kmalloc(sizeof(struct stats), GFP_KERNEL))==0){
		printk(KERN_ERR "It is not possible to allocate memory in kernel\n");
		goto rem_device;
	}
	
    printk(KERN_INFO "Init done\n");
    	 
	return 0;
	
rem_device:	
	device_destroy(simtemp_class,simtemp);
	
rem_class:
    class_destroy(simtemp_class);
    cdev_del(&simtemp_cdev);
	
rem_cdev:
	unregister_chrdev_region(simtemp,1);
	
	return -1;
	
}

/****************************************************************************
 * Exit function of the module
 ****************************************************************************/
static void __exit simtemp_exit(void)
{
	kfree(stats_storage);
	state = 2;
    wake_up(&simtemp_wq_tout);
	kthread_stop(simtemp_thread1);
	kthread_stop(simtemp_thread2);
	device_destroy(simtemp_class,simtemp);
	class_destroy(simtemp_class);
	cdev_del(&simtemp_cdev);
	unregister_chrdev_region(simtemp,1);
#ifndef SIM	
	i2c_del_driver(&simtemp_driver);
#else	
	del_timer(&simtemp_timer);
#endif	
	printk(KERN_INFO "Exit done\n");
}

/*Calling init and exit functions*/
module_init(simtemp_init);
module_exit(simtemp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Elyoenai Martinez");
MODULE_DESCRIPTION("Linux device driver for simulated temperature sensor");

