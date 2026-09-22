#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <errno.h>

/**
 * DeepHat Advanced Masking Loader (Daemon Mode)
 * 
 * This version is designed to force the process to be adopted by PID 1
 * by implementing a full daemonization sequence.
 * 
 * Usage: ./masker <fake_name> <cmd> <arg1> [arg2...]
 */

void daemonize_to_init() {
    pid_t pid;

    // Step 1: First Fork
    // This creates a child and allows the parent to exit, 
    // making the child an orphan.
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // Parent exits immediately

    // Step 2: Create a new Session
    // This detaches the process from the controlling terminal (TTY).
    // The process becomes the leader of a new session and a new process group.
    if (setsid() < 0) exit(EXIT_FAILURE);

    // Step 3: Second Fork
    // This is the "magic" for forcing PPID 1.
    // By forking again, the process is no longer a session leader.
    // This prevents it from ever acquiring a TTY again.
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // First child exits

    // Step 4: Close file descriptors
    // This ensures no connection to the terminal that launched the wrapper.
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Step 5: Change working directory to root
    // Prevents the process from "locking" the directory it was started in.
    chdir("/");

    // Step 6: Reset Umask
    // Ensures the process has a predictable file creation mask.
    umask(0);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <fake_name> <cmd> <arg1> [arg2...]\n", argv[0]);
        return 1;
    }

    char *fake_name = argv[1];
    char *actual_cmd = argv[2];

    // Force the process to become a daemon (Targeting PPID 1)
    daemonize_to_init();

    // Set the process name for 'ps' visibility
    if (prctl(PR_SET_NAME, fake_name, 0, 0, 0) < 0) {
        // Fallback if prctl fails
    }

    // Prepare arguments for the target command
    int arg_count = argc - 2;
    char **exec_args = malloc((arg_count + 1) * sizeof(char *));
    if (!exec_args) return 1;

    // We set argv[0] to the fake name to mask the command string
    exec_args[0] = fake_name;
    
    for (int i = 0; i < arg_count; i++) {
        exec_args[i + 1] = argv[i + 2];
    }
    exec_args[arg_count] = NULL;

    // Execute the actual command
    execvp(actual_cmd, exec_args);

    // If we reach here, exec failed
    return 1;
}