#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <errno.h>

/**
 * DeepHat Masking Loader (Advanced Version)
 * 
 * This tool uses a double-fork technique to daemonize the execution,
 * effectively detaching it from the terminal. It then uses prctl
 * to rename the process to a specified string.
 * 
 * Usage: ./masker <fake_name> <actual_command> <arg1> [arg2...]
 */

void daemonize() {
    pid_t pid;

    // First fork: Create a child process
    pid = fork();

    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // Parent exits

    // Create a new session to detach from the controlling terminal
    if (setsid() < 0) exit(EXIT_FAILURE);

    // Second fork: Ensures the process cannot re-acquire a TTY
    pid = fork();

    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // First child exits

    // Close standard file descriptors to prevent leaking TTY info
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <fake_name> <cmd> <arg1> [arg2...]\n", argv[0]);
        fprintf(stderr, "Example: %s [kworker/u2:1] /usr/bin/python3 arg1\n", argv[0]);
        return 1;
    }

    char *fake_name = argv[1];
    char *actual_cmd = argv[2];

    // 1. Daemonize the process to hide from the terminal/session
    daemonize();

    // 2. Set the process name using prctl
    // This changes the name seen in 'ps' and 'top'
    if (prctl(PR_SET_NAME, fake_name, 0, 0, 0) < 0) {
        // If prctl fails, we proceed anyway
    }

    // 3. Prepare the arguments for the actual command
    // We need to build a new argv array for the execvp call
    // argv[0] is the fake name, then the actual command, then its args
    int arg_count = argc - 2; // actual_cmd + its args
    char **exec_args = malloc((arg_count + 1) * sizeof(char *));

    if (!exec_args) {
        return 1;
    }

    // Set the first argument (the process name) to our fake name
    exec_args[0] = fake_name;
    
    // Fill the rest with the actual command and its arguments
    for (int i = 0; i < arg_count; i++) {
        exec_args[i + 1] = argv[i + 2];
    }
    exec_args[arg_count] = NULL;

    // 4. Execute the actual command
    // We use execvp to allow the system to find the binary in the PATH
    execvp(actual_cmd, exec_args);

    // If execvp returns, an error occurred
    // Since we are daemonized, we can't print to stdout easily
    return 1;
}