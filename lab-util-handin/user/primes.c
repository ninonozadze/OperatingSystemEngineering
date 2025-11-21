#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void primes(int read_fd) __attribute__((noreturn));

void primes(int read_fd) {
    int prime_number;
    if (read(read_fd, &prime_number, sizeof(int)) == 0) {
        exit(0);
    }
    printf("prime %d\n", prime_number);

    int pipe_fd[2];
    pipe(pipe_fd);

    if (fork() == 0) {
        close(read_fd);
        close(pipe_fd[1]);
        primes(pipe_fd[0]);
    } else {
        close(pipe_fd[0]);
        int number;

        while (read(read_fd, &number, sizeof(int))) {
            if (number % prime_number != 0) {
                write(pipe_fd[1], &number, sizeof(int));
            }
        }

        close(pipe_fd[1]);
        close(read_fd);

        wait(0);
        exit(0);
    }
}

int main() {

    int max = 280;
    int pipe_fd[2];
    pipe(pipe_fd);

    if (fork() == 0) {
        close(pipe_fd[1]);
        primes(pipe_fd[0]);

    } else {
        close(pipe_fd[0]);
        for (int i = 2; i <= max; i++) {
            write(pipe_fd[1], &i, sizeof(int));
        }

        close(pipe_fd[1]);
        wait(0);
        exit(0);
    }
}