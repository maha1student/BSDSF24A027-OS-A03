#include "shell.h"
#include <fcntl.h>

#define MAXCMDS 5

// Scans args for "<" and ">", redirects stdin/stdout, removes those tokens.
// Runs only in the child process, after fork() and before execvp().
static void apply_redirections(char** args) {
    int i = 0;
    while (args[i] != NULL) {
        if (strcmp(args[i], "<") == 0 || strcmp(args[i], ">") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "syntax error: missing filename after %s\n", args[i]);
                exit(1);
            }
            int fd;
            if (args[i][0] == '<') {
                fd = open(args[i + 1], O_RDONLY);
                if (fd < 0) { perror(args[i + 1]); exit(1); }
                dup2(fd, STDIN_FILENO);
            } else {
                fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror(args[i + 1]); exit(1); }
                dup2(fd, STDOUT_FILENO);
            }
            close(fd);

            // remove the operator and filename from args
            int j = i;
            while (args[j + 2] != NULL) {
                args[j] = args[j + 2];
                j++;
            }
            args[j] = NULL;
        } else {
            i++;
        }
    }
}

int execute(char** arglist) {
    char* copy[MAXARGS + 2];      // our own pointer array, so arglist stays intact for free()
    char** cmds[MAXCMDS];
    int ncmds = 0;
    int i;

    for (i = 0; arglist[i] != NULL; i++) copy[i] = arglist[i];
    copy[i] = NULL;

    // Split on "|" into separate commands
    cmds[ncmds++] = copy;
    for (i = 0; copy[i] != NULL; i++) {
        if (strcmp(copy[i], "|") == 0) {
            copy[i] = NULL;
            if (ncmds == MAXCMDS) {
                fprintf(stderr, "too many pipes\n");
                return -1;
            }
            cmds[ncmds++] = &copy[i + 1];
        }
    }
    for (i = 0; i < ncmds; i++) {
        if (cmds[i][0] == NULL) {
            fprintf(stderr, "syntax error near |\n");
            return -1;
        }
    }

    // Create all pipes in the parent
    int pipefd[MAXCMDS][2];
    for (i = 0; i < ncmds - 1; i++) {
        if (pipe(pipefd[i]) < 0) {
            perror("pipe failed");
            return -1;
        }
    }

    // One child per command
    pid_t pids[MAXCMDS];
    for (i = 0; i < ncmds; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork failed");
            exit(1);
        }
        if (pids[i] == 0) { // Child
            if (i > 0) dup2(pipefd[i - 1][0], STDIN_FILENO);
            if (i < ncmds - 1) dup2(pipefd[i][1], STDOUT_FILENO);

            // close ALL pipe ends in the child
            for (int k = 0; k < ncmds - 1; k++) {
                close(pipefd[k][0]);
                close(pipefd[k][1]);
            }

            apply_redirections(cmds[i]);   // < and > override the pipe
            if (cmds[i][0] == NULL) exit(1);

            execvp(cmds[i][0], cmds[i]);
            perror("Command not found");
            exit(1);
        }
    }

    // Parent: close ALL pipe ends, otherwise the reader never sees EOF and hangs
    for (i = 0; i < ncmds - 1; i++) {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }

    int status;
    for (i = 0; i < ncmds; i++) {
        waitpid(pids[i], &status, 0);
    }
    return 0;
}