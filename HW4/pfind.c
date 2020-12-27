#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <zconf.h>

#define True 1
#define False 0
#define CONTINUE_MSG "Directory %s: Permission denied.\n"
#define EXIT_MSG "Done searching, found %d files\n"

/*
 * -----Structs-------
 */
typedef struct node
{
    char directory[PATH_MAX];
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
pthread_mutex_t queue_lock, counter_lock, dead_thread_lock, sleep_thread_lock, thread_lock, start_all_lock;
pthread_cond_t queue_not_empty, start_search;

/*
 * Fields.
 */
int NumberOfThreads;
pthread_t *ThreadList;
char* Pattern;
static Queue WorkingQueue = {0, NULL, NULL};
int PatternCounter;
int SleepThreads;
int ThreadErrorsCounter;
int TotalOpened;

/*
 * Check if we can search in this directory.
 */
int DirectorySearchable(char *path)
{
    DIR *dir = opendir(path);
    if(!dir)
    {
        return 0;
    }
    else
    {
        closedir(dir);
        return 1;
    }
}

/*
 * Adding directory to queue.
 * Because it is FIFO we will elements to the end of the queue.
 */
int AddNewDirToQueue(char* queueDirectory)
{
    pthread_mutex_lock(&queue_lock);
    DirectoryNode *node = malloc(sizeof(DirectoryNode));
    if(node == NULL) {
        fprintf(stderr, "Error in allocating memory for Node.");
        pthread_mutex_unlock(&queue_lock);
        return False;
    }

    strcpy(node -> directory, queueDirectory);
    node -> next = NULL;

    if((WorkingQueue.head) == NULL &&  (WorkingQueue.tail) == NULL)
    {
        //First element of the queue.
        WorkingQueue.head = WorkingQueue.tail = node;
    }
    else
    {
        WorkingQueue.tail -> next = node;
        WorkingQueue.tail = node;
    }

    WorkingQueue.length += 1;
    pthread_mutex_unlock(&queue_lock);
    return True;
}

/*
 * Count thread as working thread.
 */
void Wakeup()
{
    pthread_mutex_lock(&sleep_thread_lock);
    SleepThreads--;
    pthread_mutex_unlock(&sleep_thread_lock);
}

/*
 * Kill all threads.
 */
void KillAll()
{
    sleep(0);
    for(int i=0; i<NumberOfThreads; i++)
    {
        pthread_cancel(ThreadList[i]);
    }
}

/*
 * Count dormant threads. Send kill command if needed.
 */
void GetDormantThreadsAndKill()
{
    sleep(0);
    pthread_mutex_lock(&sleep_thread_lock);
    pthread_mutex_lock(&dead_thread_lock);
    int total_dormant = ThreadErrorsCounter + SleepThreads;
    pthread_mutex_unlock(&sleep_thread_lock);
    pthread_mutex_unlock(&dead_thread_lock);

    if(total_dormant>NumberOfThreads)
    {
        KillAll();
    }
}

/*
 * Count thread as sleep.
 */
void Sleep()
{
    pthread_mutex_lock(&sleep_thread_lock);
    SleepThreads++;
    pthread_mutex_unlock(&sleep_thread_lock);
}

/*
 * Making thread to sleep.
 */
void WaitForData()
{
    GetDormantThreadsAndKill();

    //Waiting for signal.
    pthread_mutex_lock(&queue_lock);
    pthread_cond_wait(&queue_not_empty, &queue_lock);
    pthread_mutex_unlock(&queue_lock);
}

/*
 * Removing directory from queue.
 * Because it is FIFO we will remove the first element.
 */
void RemoveDirFromQueue(char *nexDirectory)
{
    DirectoryNode *head = WorkingQueue.head;
    DirectoryNode *second = head -> next;
    if(head->directory != NULL)
        strcpy(nexDirectory, head->directory);
    WorkingQueue.head = second;
    if(WorkingQueue.head == NULL)
    {
        //We reached final element.
        WorkingQueue.tail = WorkingQueue.head;
    }
    free(head);

    WorkingQueue.length -= 1;
    pthread_mutex_unlock(&queue_lock);
}

/*
 * If pattern was found we increasing our counter.
 */
void IncreaseCounter()
{
    pthread_mutex_lock(&counter_lock);
    PatternCounter++;
    pthread_mutex_unlock(&counter_lock);
}

/*
 * Initiating all relevant data structures for start working.
 */
void Init(int argc, char *argv[])
{
    //Check args.
    if(argc != 4)
    {
        fprintf(stderr,"Error in the number of arguments");
        exit(1);
    }

    //Init counter.
    PatternCounter = 0;
    SleepThreads = 0;
    ThreadErrorsCounter = 0;
    TotalOpened = 0;

    //Adding root to queue.
    if (DirectorySearchable(argv[1])) {
        AddNewDirToQueue(argv[1]);
    }
    else
    {
        fprintf(stderr, "Root is not searchable.\n");
        exit(1);
    }

    //Init fields.
    Pattern = argv[2];
    NumberOfThreads = atoi(argv[3]);
    ThreadList = malloc(NumberOfThreads * sizeof(pthread_t));
    if(ThreadList == NULL)
    {
        fprintf(stderr, "Error in list allocation.");
        exit(1);
    }
}

/*
 * Check if we need to ignore file.
 */
int IgnoreFile(char *file)
{
    return (strcmp(file, ".") == 0) || (strcmp(file, "..") == 0);
}

/*
 * Check if the path is the desired pattern.
 */
int EqualToPattern(char *path)
{
    return strstr(path,Pattern) != NULL;
}

/*
 * Scanning current directory content.
 */
int ScanDirectory(const char *directory)
{
    struct dirent *entry = NULL;
    struct stat f_info;
    char path[PATH_MAX];
    char file_name[FILENAME_MAX];
    int ret = False;
    DIR *dir = opendir(directory);

    if(dir == NULL){
        fprintf(stderr, "Couldn't open %s.\n", directory);
        return False;
    }
    if(errno == EACCES){
        fprintf(stderr, CONTINUE_MSG, directory);
        return False;
    }

    while((entry = readdir(dir)) != NULL)
    {
        strcpy(path, directory);
        if(path[strlen(path) -1] != '/')
            strcat(path, "/");

        strcpy(file_name, entry->d_name);
        if(IgnoreFile(file_name))
            continue;

        //Adding new file full path.
        strcat(path, entry->d_name);

        if(lstat(path, &f_info) == -1)
        {
            fprintf(stderr, "Unable get %s status.\n", path);
            ret = False;
        }

        if(S_ISDIR(f_info.st_mode))
        {
            AddNewDirToQueue(entry->d_name);
            pthread_cond_broadcast(&queue_not_empty);
            ret = True;
        }

        else
        {
            if(EqualToPattern(entry->d_name))
            {
                printf("%s\n", path);
                IncreaseCounter();
            }
            ret = True;
        }
    }

    closedir(dir);
    return ret;
}

/*
 * Kill thread for receiving error.
 */
void KillOnError()
{
    sleep(0);
    pthread_mutex_lock(&dead_thread_lock);
    ThreadErrorsCounter++;
    pthread_mutex_unlock(&dead_thread_lock);
    GetDormantThreadsAndKill();
    pthread_exit(NULL);
}

/*
 * Return true if is empty.
 */
int IsEmpty()
{
    pthread_mutex_lock(&queue_lock);
    int length = WorkingQueue.length==0;
    pthread_mutex_unlock(&queue_lock);
    return length;
}

void WaitForAllStart()
{
    pthread_mutex_lock(&thread_lock);
    TotalOpened++;
    pthread_mutex_unlock(&thread_lock);
    pthread_mutex_lock(&start_all_lock);
    pthread_cond_wait(&start_search, &start_all_lock);
    pthread_mutex_unlock(&start_all_lock);
}

/*
 * The loop will try:
 *  - Try to dequeue element.
 *  - Scan directory.
 *  - Print matching patterns.
 */
void *ThreadLoop(void *num)
{
    WaitForAllStart();

    while(True)
    {
        if(IsEmpty())
        {
            WaitForData();
        }

        Wakeup();
        char directory[PATH_MAX];
        pthread_mutex_lock(&queue_lock);
        RemoveDirFromQueue(directory);
        pthread_mutex_unlock(&queue_lock);
        ScanDirectory(directory) ? Sleep() : KillOnError();
    }
}

/*
 * Initiating all threads.
 */
void Run()
{
    for(long i = 0; i < NumberOfThreads; i++)
    {
        if(pthread_create(&ThreadList[i], NULL, ThreadLoop, (void *)i))
        {
            fprintf(stderr, "Error in creating thread.");
            pthread_mutex_lock(&counter_lock);
            ThreadErrorsCounter++;
            pthread_mutex_unlock(&counter_lock);
        }
    }

    SleepThreads = NumberOfThreads - ThreadErrorsCounter;
    while(True)
    {
        if(TotalOpened>=NumberOfThreads)
            break;
    }

    pthread_mutex_lock(&thread_lock);
    pthread_cond_broadcast(&start_search);
    pthread_mutex_unlock(&thread_lock);
}


/*
 * Starting this session locks and conditionals.
 */
void StartLocksAndConditionals()
{
    int rc = pthread_mutex_init(&queue_lock, NULL) || pthread_mutex_init(&sleep_thread_lock,NULL)
            || pthread_mutex_init(&dead_thread_lock, NULL) || pthread_mutex_init(&counter_lock,NULL) ||
            pthread_mutex_init(&thread_lock, NULL) || pthread_mutex_init(&start_all_lock, NULL);
    if(rc)
    {
        fprintf(stderr, "Error in pthread_mutex_init()");
        exit(-1);
    }

    rc = pthread_cond_init(&queue_not_empty, NULL) || pthread_cond_init(&start_search, NULL);
    if(rc)
    {
        fprintf(stderr, "Error in pthread_cond_init()");
        exit(-1);
    }
}

/*
 * Destroying this session locks and conditionals.
 */
void CleanLocksAndConditionals()
{
    pthread_mutex_destroy(&queue_lock);
    pthread_mutex_destroy(&counter_lock);
    pthread_mutex_destroy(&dead_thread_lock);
    pthread_mutex_destroy(&sleep_thread_lock);
    pthread_mutex_destroy(&thread_lock);
    pthread_mutex_destroy(&start_all_lock);

    pthread_cond_destroy(&queue_not_empty);
    pthread_cond_destroy(&start_search);
}

/*
 * Freeing all free data and memory of this session.
 */
void FreeAllMemory()
{
    free(ThreadList);
    CleanLocksAndConditionals();
}

int main(int argc, char *argv[])
{
    StartLocksAndConditionals();
    Init(argc,argv);
    Run();
    FreeAllMemory();
    printf(EXIT_MSG, PatternCounter);
    return ThreadErrorsCounter > 0;
}
