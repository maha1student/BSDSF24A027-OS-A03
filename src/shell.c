#include "shell.h"

static char* history[HISTORY_SIZE];
static int history_count = 0;

typedef struct {
    pid_t pid;
    char cmd[MAX_LEN];
} Job;

static Job jobs[MAXJOBS];
static int job_count = 0;

void add_history_cmd(char* cmdline) {
    if (history_count == HISTORY_SIZE) {
        free(history[0]);
        for (int i = 1; i < HISTORY_SIZE; i++) {
            history[i - 1] = history[i];
        }
        history_count--;
    }
    history[history_count++] = strdup(cmdline);
}

char* get_history_cmd(int n) {
    if (n < 1 || n > history_count) {
        return NULL;
    }
    return history[n - 1];
}

void add_job(pid_t pid, const char* cmd) {
    if (job_count == MAXJOBS) {
        fprintf(stderr, "too many background jobs\n");
        return;
    }
    jobs[job_count].pid = pid;
    strncpy(jobs[job_count].cmd, cmd, MAX_LEN - 1);
    jobs[job_count].cmd[MAX_LEN - 1] = '\0';
    job_count++;
}

static void remove_job(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) {
            printf("[Done] %d  %s\n", (int)pid, jobs[i].cmd);
            for (int j = i + 1; j < job_count; j++) {
                jobs[j - 1] = jobs[j];
            }
            job_count--;
            return;
        }
    }
}

// Non-blocking: reaps every finished child, returns immediately if none.
void reap_jobs(void) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        remove_job(pid);
    }
}

void print_jobs(void) {
    if (job_count == 0) {
        printf("No background jobs.\n");
        return;
    }
    for (int i = 0; i < job_count; i++) {
        printf("[%d] %d  %s\n", i + 1, (int)jobs[i].pid, jobs[i].cmd);
    }
}

char** tokenize(char* cmdline) {
    // Edge case: empty command line
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n') {
        return NULL;
    }

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    for (int i = 0; i < MAXARGS + 1; i++) {
        arglist[i] = (char*)malloc(sizeof(char) * ARGLEN);
        bzero(arglist[i], ARGLEN);
    }

    char* cp = cmdline;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++; // Skip leading whitespace

        if (*cp == '\0') break; // Line was only whitespace

        int len = 0;
        int inquote = 0;
        // Token ends at whitespace, unless we are inside "..."
        while (*cp != '\0' && (inquote || (*cp != ' ' && *cp != '\t'))) {
            if (*cp == '"') {
                inquote = !inquote;          // quote itself is not copied
            } else if (len < ARGLEN - 1) {
                arglist[argnum][len++] = *cp;
            }
            cp++;
        }
        arglist[argnum][len] = '\0';
        argnum++;
    }

    if (argnum == 0) { // No arguments were parsed
        for (int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL;
    return arglist;
}

int handle_builtin(char** arglist) {
    if (strcmp(arglist[0], "exit") == 0) {
        printf("Shell exited.\n");
        exit(0);
    }
    else if (strcmp(arglist[0], "cd") == 0) {
        if (arglist[1] == NULL) {
            fprintf(stderr, "cd: missing directory\n");
        } else if (chdir(arglist[1]) != 0) {
            perror("cd failed");
        }
        return 1;
    }
    else if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  exit        - exit the shell\n");
        printf("  cd <dir>    - change directory\n");
        printf("  help        - show this help\n");
        printf("  jobs        - list background jobs\n");
        printf("  history     - show command history\n");
        return 1;
    }
    else if (strcmp(arglist[0], "jobs") == 0) {
        print_jobs();
        return 1;
    }
    else if (strcmp(arglist[0], "history") == 0) {
        for (int i = 0; i < history_count; i++) {
            printf("%d  %s\n", i + 1, history[i]);
        }
        return 1;
    }
    return 0;
}