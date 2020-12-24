#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define True 1
#define False 0
#define CONTINUE_MSG "Directory %s: Permission denied.\n"

/*
 * -----Structs-------
 */
typedef struct node
{
    char* directory;
    struct node *next;
}DirectoryNode;

typedef struct queue{
    int length;
    DirectoryNode *head;
    DirectoryNode *tail;
}Queue;

/*
 * Locks and conditionals.
 */
pthread_mutex_t queue_lock;
pthread_cond_t queue_not_empty;

/*
 * Fields.
 */
int NumberOfThreads;
pthread_t *ThreadList;
char* pattern;
Queue *WorkingQueue;

/*
 * Adding directory to queue.
 * Because it is FIFO we will elements to the end of the queue.
 */
int AddNewDirToQueue(char* queueDirectory)
{
    pthread_mutex_lock(&queue_lock);

    DirectoryNode *node = malloc(sizeof(DirectoryNode));
    if(node == NULL){
        fprintf(stderr, "Error in allocating memory for Node.");
        return False;
    }

    node -> directory = queueDirectory;
    node -> next = NULL;

    if((WorkingQueue -> head) == NULL &&  (WorkingQueue -> tail))
    {
        //First element of the queue.
        WorkingQueue -> head = WorkingQueue -> tail = node;
        pthread_cond_signal(&queue_not_empty);
    }
    else
    {
        WorkingQueue -> tail -> next = node;
        WorkingQueue -> tail = node;
    }

    WorkingQueue -> length += 1;
    pthread_mutex_unlock(&queue_lock);
    return True;
}

/*
 * Removing directory from queue.
 * Because it is FIFO we will remove the first element.
 */
void RemoveDirFromQueue(char *nexDirectory)
{
    pthread_mutex_lock(&queue_lock);
    if((WorkingQueue -> head) == NULL &&  (WorkingQueue -> tail))
    {
        //List is empty.
        return;
    }

    DirectoryNode *head = WorkingQueue -> head;
    DirectoryNode *second = head -> next;
    if(head->directory != NULL)
        strcpy(nexDirectory, head->directory);
    WorkingQueue -> head = second;
    if(WorkingQueue -> head == NULL)
    {
        //We reached final element.
        WorkingQueue -> tail = WorkingQueue -> head;
    }
    free(head->directory);
    free(head);

    WorkingQueue -> length -= 1;
    pthread_mutex_unlock(&queue_lock);
    return;
}

/*
 * Init the queue.
 */
void InitQueue()
{
    WorkingQueue = malloc(sizeof(Queue));
    WorkingQueue -> length = 0;
    WorkingQueue -> head = NULL;
    WorkingQueue -> tail = NULL;
}

/*
 * Initiating all relevant data structures for start working.
 */
void Init(int argc, char *argv[])
{
    if(argc != 4)
    {
        fprintf(stderr,"Error in the number of arguments");
        exit(-1);
    }
    pattern = argv[2];
    NumberOfThreads = atoi(argv[3]);
    ThreadList = malloc(NumberOfThreads * sizeof(pthread_t));
    InitQueue();
    //Adding root to queue.
    AddNewDirToQueue(argv[1]);
}

int QueueEmpty()
{
    int status = -1;
    pthread_mutex_lock(&queue_lock);
    status = WorkingQueue ->length;
    pthread_mutex_unlock(&queue_lock);
    return (status == 0);
}

int IgnoreFile(*file)
{
    return strcmp(file, ".") == 0 || (file, "..") == 0;
}

void ScanDirectory(const char *directory)
{
    struct dirent *entry = NULL;
    struct stat f_info;
    char path[PATH_MAX];
    char file_name[FILENAME_MAX];
    int ret;
    DIR *dir = opendir(directory);

    if(dir == NULL){
        fprintf(stderr, "Couldn't open %s.\n", directory);
    }

    strcpy(path, directory);
    strcat(path, "/");

    while((entry = readdir(dir)) != NULL)
    {
        strcpy(file_name, entry->d_name);
        if(IgnoreFile(file_name))
            continue;
    }

}

/*
 * The loop will try:
 *  - Try to dequeue element.
 *  - Scan directory.
 *  - Print matching patterns.
 */
void *ThreadLoop()
{
    while(True)
    {
        if(QueueEmpty)
        {
            //Wait for data?
        }
        else
        {
            char *directory;
            RemoveDirFromQueue(directory);
            ScanDirectory(directory);
        }

        break;
    }
}

void StartThreads()
{
    for(long t = 0; t<NumberOfThreads; NumberOfThreads++)
    {
        if(pthread_create(NumberOfThreads, NULL, ThreadLoop, NULL))
        {
            fprintf(stderr, "Error in creating thread.");
            exit(-1);
        }
    }
}

void StartLocksAndConditionals()
{
    int rc = pthread_mutex_init(&queue_lock, NULL);
    if(rc)
    {
        fprintf(stderr, "Error in pthread_mutex_init()");
        exit(-1);
        //TODO: Check error.
    }

    rc = pthread_cond_init(&queue_not_empty, NULL);
    if(rc)
    {
        fprintf(stderr, "Error in pthread_cond_init()");
        exit(-1);
    }
}

void CleanLocksAndConditionals()
{
    pthread_mutex_destroy(&queue_lock);
    pthread_cond_destroy(&queue_not_empty);
}

void FreeAllMemory()
{
    CleanLocksAndConditionals();
    free(WorkingQueue);
    free(ThreadList);
}

int main(int argc, char *argv[])
{
    Init(argc,argv);
    StartLocksAndConditionals();
    StartThreads();
    FreeAllMemory();
}
