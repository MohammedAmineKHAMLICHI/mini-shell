// Auteur : Mohammed Amine KHAMLICHI
// LinkedIn : https://www.linkedin.com/in/mohammedaminekhamlichi/
// minishell.c
// Mini-shell Unix : processus, signaux, pipes, redirections, builtins cd/exit
// Auteur : Mohammed Amine KHAMLICHI

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

typedef struct {
    char **argv;      // arguments NULL-terminated
    char *input;      // fichier pour '<'
    char *output;     // fichier pour '>' ou '>>'
    int append;       // 0 => '>', 1 => '>>'
} Command;

static volatile sig_atomic_t fg_pgid = -1; // process group ID de la commande au premier plan
static pid_t shell_pgid = -1;
static bool shell_is_interactive = false;

// ---------- Gestion m?moire des commandes ----------

// Libère les allocations internes d'une commande (argv, redirections).
static void free_command(Command *cmd) {
    if (!cmd) return;
    if (cmd->argv) {
        for (size_t i = 0; cmd->argv[i]; ++i) {
            free(cmd->argv[i]);
        }
        free(cmd->argv);
    }
    free(cmd->input);
    free(cmd->output);
}

// Libère un tableau de commandes parsées.
static void free_commands(Command *cmds, int n) {
    if (!cmds) return;
    for (int i = 0; i < n; ++i) {
        free_command(&cmds[i]);
    }
    free(cmds);
}

// ---------- Parsing ----------

// Trim spaces en debut/fin (sur une copie)
static char *trim_ws(char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\n') s++;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n')) {
        *end-- = '\0';
    }
    return s;
}

// Parse un segment de pipeline (sans '|') en Command
// Parse un segment de pipeline (sans '|') en structure Command.
static int parse_segment(char *segment, Command *cmd) {
    cmd->argv = NULL;
    cmd->input = NULL;
    cmd->output = NULL;
    cmd->append = 0;

    size_t argv_cap = 0, argc = 0;

    char *saveptr = NULL;
    char *token = strtok_r(segment, " \t\r\n", &saveptr);

    while (token) {
        if (strcmp(token, "<") == 0) {
            token = strtok_r(NULL, " \t\r\n", &saveptr);
            if (!token) {
                fprintf(stderr, "Erreur de syntaxe: fichier attendu apres '<'\n");
                free_command(cmd);
                return -1;
            }
            cmd->input = strdup(token);
            if (!cmd->input) {
                perror("strdup");
                free_command(cmd);
                return -1;
            }
        } else if (strcmp(token, ">") == 0 || strcmp(token, ">>") == 0) {
            int append = (token[1] == '>');
            token = strtok_r(NULL, " \t\r\n", &saveptr);
            if (!token) {
                fprintf(stderr, "Erreur de syntaxe: fichier attendu apres '>'\n");
                free_command(cmd);
                return -1;
            }
            cmd->output = strdup(token);
            if (!cmd->output) {
                perror("strdup");
                free_command(cmd);
                return -1;
            }
            cmd->append = append;
        } else {
            if (argc + 1 >= argv_cap) {
                argv_cap = argv_cap ? argv_cap * 2 : 8;
                char **new_argv = realloc(cmd->argv, argv_cap * sizeof(char *));
                if (!new_argv) {
                    perror("realloc");
                    free_command(cmd);
                    return -1;
                }
                cmd->argv = new_argv;
            }
            cmd->argv[argc] = strdup(token);
            if (!cmd->argv[argc]) {
                perror("strdup");
                free_command(cmd);
                return -1;
            }
            argc++;
            cmd->argv[argc] = NULL;
        }

        token = strtok_r(NULL, " \t\r\n", &saveptr);
    }

    if (!cmd->argv || !cmd->argv[0]) {
        fprintf(stderr, "Commande vide\n");
        free_command(cmd);
        return -1;
    }
    return 0;
}

// Parse une ligne compl?te en tableau de Command (pipeline)
static int parse_line(const char *line, Command **out_cmds) {
    *out_cmds = NULL;
    int ncmds = 0;
    int cap = 0;

    char *copy = strdup(line);
    if (!copy) {
        perror("strdup");
        return -1;
    }

    char *saveptr = NULL;
    char *segment = strtok_r(copy, "|", &saveptr);

    while (segment) {
        segment = trim_ws(segment);
        if (*segment == '\0') {
            fprintf(stderr, "Erreur de syntaxe: segment de pipeline vide\n");
            free_commands(*out_cmds, ncmds);
            free(copy);
            *out_cmds = NULL;
            return -1;
        }

        if (ncmds == cap) {
            cap = cap ? cap * 2 : 4;
            Command *new_cmds = realloc(*out_cmds, cap * sizeof(Command));
            if (!new_cmds) {
                perror("realloc");
                free_commands(*out_cmds, ncmds);
                free(copy);
                *out_cmds = NULL;
                return -1;
            }
            *out_cmds = new_cmds;
        }

        if (parse_segment(segment, &(*out_cmds)[ncmds]) != 0) {
            free_commands(*out_cmds, ncmds);
            free(copy);
            *out_cmds = NULL;
            return -1;
        }
        ncmds++;

        segment = strtok_r(NULL, "|", &saveptr);
    }

    free(copy);
    return ncmds;
}

// ---------- Builtins ----------

static int builtin_cd(Command *cmd) {
    const char *path = cmd->argv[1];
    if (!path) {
        path = getenv("HOME");
        if (!path) path = ".";
    }
    if (chdir(path) != 0) {
        perror("cd");
        return -1;
    }
    return 0;
}

static int handle_builtin(Command *cmd) {
    if (!cmd->argv || !cmd->argv[0]) return 0;

    if (strcmp(cmd->argv[0], "cd") == 0) {
        builtin_cd(cmd);
        return 1;
    } else if (strcmp(cmd->argv[0], "exit") == 0) {
        exit(0);
    }
    return 0;
}

// ---------- Signaux et terminal ----------

static void set_foreground_pgid(pid_t pgid) {
    if (!shell_is_interactive || pgid <= 0) return;
    if (tcsetpgrp(STDIN_FILENO, pgid) < 0 && errno != ENOTTY) {
        perror("tcsetpgrp");
    }
}

static void sigint_handler(int sig) {
    (void)sig;
    if (fg_pgid > 0) {
        kill(-fg_pgid, SIGINT);
    } else {
        write(STDOUT_FILENO, "\n", 1);
    }
}

static void init_shell_state(void) {
    shell_is_interactive = isatty(STDIN_FILENO);
    if (!shell_is_interactive) return;

    shell_pgid = getpgrp();
    if (setpgid(0, 0) < 0 && errno != EACCES) {
        perror("setpgid");
    }

    struct sigaction sa_ignore = {0};
    sa_ignore.sa_handler = SIG_IGN;
    sigaction(SIGTTOU, &sa_ignore, NULL);
    sigaction(SIGTTIN, &sa_ignore, NULL);

    shell_pgid = getpgrp();
    set_foreground_pgid(shell_pgid);
}

// ---------- Execution ----------

// Exécute un pipeline de n commandes avec pipes/redirections et gestion des jobs.
static void execute_pipeline(Command *cmds, int ncmds) {
    int prev_fd_read = -1;
    pid_t pgid = -1;
    bool spawn_failed = false;

    for (int i = 0; i < ncmds; ++i) {
        int pipefd[2] = {-1, -1};

        if (i < ncmds - 1) {
            if (pipe(pipefd) < 0) {
                perror("pipe");
                spawn_failed = true;
                break;
            }
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            spawn_failed = true;
            if (pipefd[0] != -1) close(pipefd[0]);
            if (pipefd[1] != -1) close(pipefd[1]);
            break;
        } else if (pid == 0) {
            struct sigaction sa_child = {0};
            sa_child.sa_handler = SIG_DFL;
            sigaction(SIGINT, &sa_child, NULL);

            if (pgid == -1) pgid = getpid();
            setpgid(0, pgid);

            if (prev_fd_read != -1) {
                if (dup2(prev_fd_read, STDIN_FILENO) < 0) {
                    perror("dup2 prev_fd_read");
                    _exit(1);
                }
            }
            if (pipefd[1] != -1) {
                if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
                    perror("dup2 pipefd[1]");
                    _exit(1);
                }
            }

            if (cmds[i].input) {
                int fd = open(cmds[i].input, O_RDONLY);
                if (fd < 0) {
                    perror("open input");
                    _exit(1);
                }
                if (dup2(fd, STDIN_FILENO) < 0) {
                    perror("dup2 input");
                    _exit(1);
                }
                close(fd);
            }
            if (cmds[i].output) {
                int flags = O_WRONLY | O_CREAT;
                flags |= cmds[i].append ? O_APPEND : O_TRUNC;
                int fd = open(cmds[i].output, flags, 0644);
                if (fd < 0) {
                    perror("open output");
                    _exit(1);
                }
                if (dup2(fd, STDOUT_FILENO) < 0) {
                    perror("dup2 output");
                    _exit(1);
                }
                close(fd);
            }

            if (prev_fd_read != -1) close(prev_fd_read);
            if (pipefd[0] != -1) close(pipefd[0]);
            if (pipefd[1] != -1) close(pipefd[1]);

            execvp(cmds[i].argv[0], cmds[i].argv);
            perror("execvp");
            _exit(127);
        } else {
            if (pgid == -1) pgid = pid;
            if (setpgid(pid, pgid) < 0 && errno != EACCES) {
                perror("setpgid");
            }

            if (pipefd[1] != -1) close(pipefd[1]);
            if (prev_fd_read != -1) close(prev_fd_read);
            prev_fd_read = pipefd[0];
        }
    }

    if (prev_fd_read != -1) close(prev_fd_read);

    if (pgid == -1) return;

    fg_pgid = pgid;
    set_foreground_pgid(pgid);

    if (spawn_failed) {
        kill(-pgid, SIGTERM);
    }

    int status;
    pid_t w;
    do {
        w = waitpid(-pgid, &status, 0);
    } while (w > 0 || (w < 0 && errno == EINTR));

    set_foreground_pgid(shell_pgid);
    fg_pgid = -1;
}

// ---------- Boucle principale ----------

int main(void) {
    init_shell_state();

    struct sigaction sa = {0};
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);

    char *line = NULL;
    size_t len = 0;

    while (1) {
        if (isatty(STDIN_FILENO)) {
            printf("mini-shell> ");
            fflush(stdout);
        }

        ssize_t nread = getline(&line, &len, stdin);
        if (nread < 0) {
            if (feof(stdin)) {
                printf("\n");
                break;
            } else if (errno == EINTR) {
                clearerr(stdin);
                continue;
            } else {
                perror("getline");
                break;
            }
        }

        char *trimmed = trim_ws(line);
        if (*trimmed == '\0') continue;

        Command *cmds = NULL;
        int ncmds = parse_line(trimmed, &cmds);
        if (ncmds < 0) {
            continue;
        }

        if (ncmds == 1 && handle_builtin(&cmds[0])) {
            free_commands(cmds, ncmds);
            continue;
        }

        execute_pipeline(cmds, ncmds);
        free_commands(cmds, ncmds);
    }

    free(line);
    return 0;
}
