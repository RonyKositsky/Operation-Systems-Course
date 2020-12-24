#include "message_slot.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]){
    if(argc != 4){
        exit(1);
    }

    char* FilePath= NULL;
    char* Message = NULL;
    unsigned long Channel;
    int file_desc, ret_val, length;

    FilePath = argv[1];
    Channel = atoi(argv[2]);
    Message = argv[3];
    length = strlen(Message);

    file_desc = open(FilePath, O_WRONLY);
    if(file_desc < 0){
        perror("Error in opening file");
        exit(1);
    }

    ret_val = ioctl(file_desc,MSG_SLOT_CHANNEL,Channel);
    if(ret_val < 0){
        perror("Error in opening device");
        exit(1);
    }

    ret_val = write(file_desc,Message,length);
    if(ret_val != length){
        perror("Error in writing message");
        exit(1);
    }

    ret_val = close(file_desc);
    if(ret_val<0){
        perror("Error in closing device");
        exit(1);
    }

    close(file_desc);
    return 0;
}