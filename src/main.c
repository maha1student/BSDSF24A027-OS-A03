#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

// Runs one command (no ';' inside). Detects trailing '&'.
static int run_command(char* segment) {
    int status = 0;
    char** arglist = tokenize(segment);
    if (arglist == NULL) return 0;

    int last = 0;
    while (arglist[last + 1] != NULL) last++;

    int background = 0;
    if (strcmp(arglist[last], "&") == 0) {
        background = 1;
        free(arglist[last]);
        arglist[last] = NULL;
    }

    if (arglist[0] == NULL) {
        fprintf(stderr, "syntax error: nothing to run before &\n");
        status = 1;
    } else if (handle_builtin(arglist) == 0) {
        status = execute(arglist, background);
    }

    for (int i = 0; arglist[i] != NULL; i++) {
        free(arglist[i]);
    }
    free(arglist);
    return status;
}

// Splits a line on ';' and runs each command one after another.
static int run_line(char* line) {
    int status = 0;
    char* copy = strdup(line);
    char* saveptr;
    for (char* seg = strtok_r(copy, ";", &saveptr); seg != NULL;
         seg = strtok_r(NULL, ";", &saveptr)) {
        status = run_command(seg);
    }
    free(copy);
    return status;
}

int main() {
    char* cmdline;

    while (1) {
        reap_jobs();   // collect finished background children (zombies)

        cmdline = readline(PROMPT);
        if (cmdline == NULL) break;

        if (cmdline[0] == '\0') {
            free(cmdline);
            continue;
        }

        // Handle !n before tokenizing and before adding to history
        if (cmdline[0] == '!') {
            int n = atoi(cmdline + 1);
            char* cmd = get_history_cmd(n);
            if (cmd == NULL) {
                fprintf(stderr, "history: !%s: event not found\n", cmdline + 1);
                free(cmdline);
                continue;
            }
            free(cmdline);
            cmdline = strdup(cmd);
            printf("%s\n", cmdline);
        }

        add_history(cmdline);
        add_history_cmd(cmdline);

        run_line(cmdline);
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}