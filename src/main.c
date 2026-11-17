#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

#define MAXBLOCK 20

static char* skip_ws(char* s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

// Runs one command (no ';' inside). Detects trailing '&'. Returns exit status.
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

// Splits a line on ';' and runs each command. Returns status of the last one.
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

// cond_line is the text after "if". Reads following lines until "fi".
static void handle_if(char* cond_line) {
    char* cond = strdup(cond_line);
    char* then_block[MAXBLOCK];
    char* else_block[MAXBLOCK];
    int nthen = 0, nelse = 0;
    int state = 0;      // 0 = before then, 1 = in then block, 2 = in else block
    int closed = 0;
    char* line;

    while ((line = readline("> ")) != NULL) {
        char* t = skip_ws(line);
        size_t n = strlen(t);
        while (n > 0 && (t[n - 1] == ' ' || t[n - 1] == '\t')) t[--n] = '\0';

        if (strcmp(t, "fi") == 0) {
            closed = 1;
            free(line);
            break;
        } else if (strcmp(t, "then") == 0) {
            state = 1;
        } else if (strcmp(t, "else") == 0) {
            state = 2;
        } else if (t[0] != '\0') {
            if (state == 1 && nthen < MAXBLOCK) then_block[nthen++] = strdup(t);
            else if (state == 2 && nelse < MAXBLOCK) else_block[nelse++] = strdup(t);
        }
        free(line);
    }

    if (!closed || state == 0) {
        fprintf(stderr, "syntax error: if needs 'then' ... 'fi'\n");
    } else if (skip_ws(cond)[0] == '\0') {
        fprintf(stderr, "syntax error: missing command after if\n");
    } else if (run_line(cond) == 0) {          // exit status 0 = success
        for (int i = 0; i < nthen; i++) run_line(then_block[i]);
    } else {
        for (int i = 0; i < nelse; i++) run_line(else_block[i]);
    }

    free(cond);
    for (int i = 0; i < nthen; i++) free(then_block[i]);
    for (int i = 0; i < nelse; i++) free(else_block[i]);
}

int main() {
    char* cmdline;

    while (1) {
        reap_jobs();

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

        char* t = skip_ws(cmdline);
        if (strncmp(t, "if", 2) == 0 && (t[2] == ' ' || t[2] == '\t' || t[2] == '\0')) {
            handle_if(t + 2);
        } else {
            run_line(cmdline);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}