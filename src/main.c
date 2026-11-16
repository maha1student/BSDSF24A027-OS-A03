#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

int main() {
    char* cmdline;
    char** arglist;

    while ((cmdline = readline(PROMPT)) != NULL) {
        // Skip empty lines
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

        add_history(cmdline);       // readline: up/down arrows
        add_history_cmd(cmdline);   // our own: history builtin and !n

        if ((arglist = tokenize(cmdline)) != NULL) {
            if (handle_builtin(arglist) == 0) {
                execute(arglist);
            }

            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}