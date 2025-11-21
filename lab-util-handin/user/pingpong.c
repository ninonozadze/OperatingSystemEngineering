#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int main(int argc, char* argv[]) {

    int pipe_fd[2];
    pipe(pipe_fd);

    int child_id = fork();

    if (child_id == 0){
        // Child process
        close(pipe_fd[1]);  // Close unused write end of pipe

        int message;
        read(pipe_fd[0], &message, sizeof(int));
        int pid = getpid();
//        the child should print "<pid>: received ping"
        printf("%d: received ping\n", pid);
        close(pipe_fd[0]);  // Close read end after use
    }else{
        // Parent process
        close(pipe_fd[0]);  // Close unused read end of pipe

        int message = 1;
        write(pipe_fd[1], &message, sizeof(int));
        wait(0); // Wait for the child to finish
        int pid = getpid();
//        the parent should read the byte from the child, print "<pid>: received pong"
        printf("%d: received pong\n", pid);
        close(pipe_fd[1]); // Close write end after use
    }
    return 0;
}