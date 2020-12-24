#undef __KERNEL__
#define __KERNEL__
#undef __MODULE__
#define __MODULE__
#include "message_slot.h"
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
MODULE_LICENSE("GPL");

/*
 * We will hold linked list to manage all the messages.
 */
typedef struct slot_node{
    struct slot_node *next;
    long channel;
    int length;
    char msg[MAX_MSG_BYTES];
}node;

/*
 * Static list that we will update during the runtime of the kernel.
 */
static node *messages[MAX_SLOTS_NUMBER]={0};

/*
 * Static array which holds the number of channels in each slot.
 */
static long CounterArray[MAX_SLOTS_NUMBER];

/*
 * Finding message by slot and and channel.
 */
static int GetNode(int minor, long channel,node **NodePointer){
    node *WantedNode;
    if(messages[minor] == NULL){
        *NodePointer = NULL;
        return FALSE;
    }

    WantedNode = messages[minor];
    while(WantedNode != NULL){
        if(WantedNode->channel == channel){
            *NodePointer = WantedNode;
            return TRUE;
        }

        WantedNode = WantedNode->next;
    }
    *NodePointer = NULL;
    return FALSE;
}

/*
 * Adding new massage slot and id. If slot is NULL we create it, else we are adding new channel.
 * We will check channel does not exist before calling the function.
 */
static int AddNode(int minor, long channel, node **NodePointer){
    node *NewNode = NULL;
    node *head = NULL;

    if(CounterArray[minor] >= CHANNEL_NUMBER){
        printk(KERN_ERR "We support only 2^20 channels per slot.\n");
        return FAIL;
    }

    NewNode = (node*)kmalloc(sizeof(node), GFP_KERNEL);
    if(NewNode == NULL){
        printk(KERN_ERR "Can't create new message slot.\n");
        return FAIL;
    }

    NewNode -> length = 0;
    NewNode -> next = NULL;
    NewNode -> channel = channel;
    *NodePointer = NewNode;
    if(messages[minor] != 0){
        head = messages[minor];
        NewNode->next = head;
    }
    messages[minor] = NewNode;
    CounterArray[minor] +=1;
    return SUCCESS;
}

/*
 * Custom ioctl command.
 */
static long device_ioctl(struct file *file,unsigned int ioctl_command_id,unsigned long ioctl_command_param){
    if(file == NULL){
        printk(KERN_ERR "Error in ioctl arguments");
        return -EINVAL;
    }

    if(ioctl_command_param == 0){
        printk(KERN_ERR "0 id not supported");
        return -EINVAL;
    }

    if(ioctl_command_id != MSG_SLOT_CHANNEL){
        printk(KERN_ERR "Not supported");
        return -EINVAL;
    }

    file->private_data = (void*)ioctl_command_param;
    return SUCCESS;
}

/*
 * Device write function.
 */
static ssize_t device_write(struct file *file,const char __user* buffer,size_t length,loff_t* offset){
    int ret_val, minor;
    long channel;
    node *WorkNode = NULL;
    if(file == NULL || buffer == NULL){
        printk(KERN_ERR "Error in device_write arguments.\n");
        return -EINVAL;
    }

    channel = (long)file->private_data;
    if(channel  == 0){
        printk(KERN_ERR "Please set ioctl first.\n");
        return -EINVAL;
    }

    if(length > MAX_MSG_BYTES || length == 0){
        printk(KERN_ERR "Massage size is illegal.\n");
        return -EMSGSIZE;
    }

    minor = iminor(file->f_inode);

    ret_val = GetNode(minor, channel, &WorkNode);
    if (!ret_val){ //Node wasn't found.
        ret_val = AddNode(minor, channel, &WorkNode);
        if(ret_val){ //Error in making node.
            return ret_val; 
        }
    }
    WorkNode->length = length;

    if(copy_from_user((WorkNode->msg), buffer, length)){
        return -EINVAL;
    }

    return WorkNode->length;
}

static ssize_t device_read(struct file *file, char __user* buffer, size_t length,loff_t* offset){
    int  minor, ret_val;
    long channel;
    node *FoundedNode = NULL;

    if(file == NULL || buffer == NULL){
        printk(KERN_ERR "Error in device_read arguments.\n");
        return -EINVAL;
    }

    minor = iminor(file->f_inode);
    channel = (long)(file->private_data);
    if(channel  == 0){
        printk(KERN_ERR "Please set ioctl first.\n");
        return -EINVAL;
    }

    ret_val = GetNode(minor, channel, &FoundedNode);
    if(!ret_val){
        printk(KERN_ERR "Channel wasn't found.");
        return -EWOULDBLOCK;
    }

    if(FoundedNode->length > length){
        printk(KERN_ERR "Msg read too long.\n");
        return -ENOSPC;
    }

    if(FoundedNode->length == 0){
        printk(KERN_ERR "No massage found.\n");
        return -EWOULDBLOCK;
    }

    if(copy_to_user(buffer, (FoundedNode -> msg), FoundedNode->length)){
        printk(KERN_ERR "Error in copying to user.\n");
        return -EINVAL;
    }

    return FoundedNode->length;
}

/*
 * Device open function.
 */
static int device_open(struct inode *inode,struct file *file){
    if(file == NULL || inode == NULL){
        printk(KERN_ERR "Error in device_open arguments.\n");
        return -EINVAL;
    }
    file->private_data = (void*)0;
    return SUCCESS;
}


/*
 * File operation struct.
 */
struct file_operations Fops = {
        .owner            = THIS_MODULE,
        .write            = device_write,
        .open             = device_open,
        .read             = device_read,
        .unlocked_ioctl   = device_ioctl
};

/*
 * Init counter array.
 */
static void CounterInit(void){
    int i;
    for(i=0; i <MAX_SLOTS_NUMBER; i++){
        CounterArray[i] = 0;
    }
}

/*
 * Init function.
 */
static int __init simple_init(void){
    int rc;
    rc = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &Fops);
    if(rc < 0){
        printk(KERN_ERR "Registration failed for %d.\n",  MAJOR_NUM);
        return rc;
    }
    printk(KERN_INFO "Registration succeeded for %d.\n",  MAJOR_NUM);
    CounterInit();
    return SUCCESS;
}

/*
 * Free list allocation.
 */
static void ListCleanUp(void){
    node *WorkingNode = NULL;
    node *tmp = NULL;
    int i;
    for(i = 0; i < MAX_MSG_BYTES; i++){
        WorkingNode = messages[i];
        while(WorkingNode != NULL){
            tmp = WorkingNode;
            WorkingNode = WorkingNode->next;
            kfree(tmp);
        }
    }
}

/*
 * Exit function.
 */
static void __exit simple_cleanup(void){
    //Free slots.
    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
    ListCleanUp();
}

module_init(simple_init);
module_exit(simple_cleanup);