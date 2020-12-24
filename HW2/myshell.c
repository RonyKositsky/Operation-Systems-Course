#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

// 205817893

/*
 * Allowing child signal to run.
 */
void sig_handler(int signal){
    while(waitpid((pid_t)(-1),NULL, WNOHANG) != -1){};
    if(errno != EINTR && errno != ECHILD){
        perror("Error in waiting for child process.");
        exit(1);
    }
}

/*
 * When we call child process, it can be cancelled. We need to enable it in child process.
 */
void restoring_sigint_signal(){
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGINT, &sa , 0) == -1){
        perror("Error in restoring SIGINT for child process.");
        exit(1);
    }
}

/*
 * Executing command and checking if errors happened.
 */
void execute_command_and_check_error(char** arguments){
    execvp(arguments[0], arguments);
    perror("Error in executing command.");
    exit(1);
}

/*
 * For process which ends with "&".
 */
int background_worker(char** arglist, int count){
    int pid = fork();
    arglist[count - 1] = NULL; //We ignore "&"
    if(pid < 0){ //fork failed
        perror("Failed to conduct fork() function.");
        exit(1);
    }else if(pid ==0){ //Child process
        execute_command_and_check_error(arglist);
    }

    return 1;
}

/*
 * Regular process. Shell waits until it's finished.
 */
int regular_worker(char** arglist){
    pid_t pid = fork();
    if(pid < 0){ //fork failed
        printf("Failed to conduct fork() function.");
        exit(1);
    }else if(pid ==0){ //Child process
        restoring_sigint_signal();
        execute_command_and_check_error(arglist);
    }else{
        if(waitpid(pid,NULL,WUNTRACED) == -1){ //Wait for process.
            return 0;
        }
    }
    return 1;
}


/*
 * Setting the reading end of the pipeline.
 */
void reading_end(int* pipefd, char** arglist){
    dup2(pipefd[0], STDIN_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);
    restoring_sigint_signal();
    execute_command_and_check_error(arglist);
}

/*
 * Setting the writing end of the pipeline.
 */
void writing_end(int* pipefd, char** arglist, int pointer){
    dup2(pipefd[1], STDIN_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);

    //We need to set the second set of instructions.
    //We do it with pointer arithmetic.
    char** second_argument = arglist + pointer +1;
    restoring_sigint_signal();
    execute_command_and_check_error(second_argument);
}

/*
 * Pipeline process. Working in the background.
 */
int pipeline_worker(char** arglist, int pointer){
    int pipefd[2];
    if(pipe(pipefd)==-1){
        perror("Error in pipe() function");
        exit(1);
    }

    pid_t pid = fork();

    if(pid < 0){
        perror("Failed to conduct fork() function inside pipeline.");
        exit(1);
    }

    if(pid == 0){ //Child read from pipe.
        reading_end(pipefd, arglist);
    }else{ //Parent write to file
        pid_t pid2 = fork();
        if(pid < 0){
            perror("Failed to conduct fork() function inside pipeline.");
            exit(1);
        }else if(pid == 0){ // Grandchild to write to pipe.
            writing_end(pipefd, arglist, pointer);
        }else{
            if(waitpid(pid,NULL,WUNTRACED) == -1){ //Waiting for process.
                return 0;
            };
            if(waitpid(pid2,NULL,WUNTRACED) == -1){ //Waiting for process.
                return 0;
            };
            return 1;
        }
    }
}


/*
 * arglist runner. Decides which tpye of process to run,
 */
int process_arglist(int count, char** arglist){
    if(strcmp(arglist[count -1],"&") == 0) //Check if it is background process.
        return background_worker(arglist, count);
    else{
        for(int i=0; i< count; i++){
            if(strcmp(arglist[i],"|") == 0){
                arglist[i] = NULL; //Setting it to NULL for execvp() function.
                return pipeline_worker(arglist, i);
            }
        }
        return regular_worker(arglist);
    }
}

/*
 * Init sigaction.
 */
int init_sigaction(){
    struct sigaction child_sa;

    //Prepare child
    child_sa.sa_handler = &sig_handler;
    child_sa.sa_flags = SA_RESTART;
    if(sigaction(SIGCHLD,&child_sa,0) == -1){ //error
        perror("Error in child sigaction");
        return 1;
    }

    //Prepare parent.
    struct sigaction sa;

    sa.sa_handler = SIG_IGN; //Ignoring signals from main loop.
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGINT,&sa,0) == -1){    ; //Init the signal set point.
        perror("Error in main sigaction");
        return 1;
    }

    return 0;
}

int prepare(void){
    return init_sigaction();
}

/*
 * Stop shell.
 */
int finalize(void){
    return 0;
}


