#include "message_slot.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]){
    char* FilePath;
    char Message[MAX_MSG_BYTES];
    unsigned long Channel;
    int file_desc, ret_val;

    if(argc != 3){
        exit(1);
    }

    FilePath = argv[1];
    Channel = atoi(argv[2]);

    file_desc = open(FilePath, O_RDONLY);
    if(file_desc < 0){
        perror("Error in opening file");
        exit(1);
    }

    ret_val = ioctl(file_desc,MSG_SLOT_CHANNEL,Channel);
    if(ret_val < 0){
        perror("Error in opening device");
        exit(1);
    }

    ret_val = read(file_desc,Message,strlen(Message));
    if(ret_val < 0){
        perror("Error in reading massage");
        exit(1);
    }else{
        int status = write(STDOUT_FILENO, Message, ret_val);
        if(status == -1){
            perror("Error in writing to console");
            exit(1);
        }
    }

    ret_val = close(file_desc);
    if(ret_val<0){
        perror("Error in closing device");
        exit(1);
    }

    close(file_desc);
    return 0;
}