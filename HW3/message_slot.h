#ifndef MESSAGE_SLOT
#define MESSAGE_SLOT

#include <linux/ioctl.h>
#define MAJOR_NUM 240
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned long)
#define MAX_MSG_BYTES 128
#define MAX_SLOTS_NUMBER 256
#define SUCCESS 0
#define FAIL -1
#define CHANNEL_NUMBER 1048576
#define DEVICE_RANGE_NAME "message_slot"
#define TRUE 1
#define FALSE 0

#endif