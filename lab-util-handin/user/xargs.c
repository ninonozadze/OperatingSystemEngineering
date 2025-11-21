#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAX_LINE_LEN 512

void execute_command(char *command, char **args) {
    if (fork() == 0) {
        exec(command, args);
        fprintf(2, "exec %s failed\n", command);
        exit(1);
    } else {
        wait(0);
    }
}

int main(int argc, char *argv[]) {
    char lines[MAX_LINE_LEN];
    char *args[MAXARG];

    for (int i = 1; i < argc; i++) {
        args[i-1] = argv[i];
    }
    int arg_cnt = argc - 1;

    while (1) {
        int idx = 0;
        int num;

        while ((num = read(0, &lines[idx], 1)) > 0) {
            if (lines[idx] == '\n') break;
            if (++idx >= MAX_LINE_LEN - 1) {
                fprintf(2, "Line is too long\n");
                exit(1);
            }
        }

        if (idx == 0 && num <= 0) break;

        lines[idx] = 0;

        char *arg = lines;
        while (*arg && arg_cnt < MAXARG - 1) {
            args[arg_cnt++] = arg;
            while (*arg && *arg != ' ') arg++;
            if (*arg == ' ') {
                *arg++ = 0;
                while (*arg == ' ') arg++;
            }
        }
        args[arg_cnt] = 0;

        execute_command(args[0], args);

        arg_cnt = argc - 1;
    }

    exit(0);
}