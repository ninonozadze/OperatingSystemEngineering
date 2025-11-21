#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int main(int argc, char* argv[]) {
    // Check if exactly 2 arguments are provided
    if (argc != 2) {
        // 2 is the file descriptor for stderr (standard error)
        // 6 specifies the number of bytes to write
        // in this case, the string "argument error\n" has 15 characters (14 letters and 1 newline character)
        write(2, "argument error\n", 15);
    }
    //    "The command-line argument is passed as a string; you can convert it to an integer using atoi (see user/ulib.c)."
    // Convert command-line argument from string to integer
    int sleep_time = atoi(argv[1]);

    if(sleep_time < 0){
        printf("sleep time should be non-negative");
    }

    //    "Use the system call sleep"
    sleep(sleep_time);

    //    "sleep's main should call exit(0) when it is done."
    //    the program exits with a status code of 0, indicating successful execution
    exit(0);
}