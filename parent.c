#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

static char CHILD_PROCESS_NAME[] = "child";

int main(int argc, char **argv) {
    if (argc != 1) {
        const char msg[] = "Usage: ./parent\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    const char filename_prompt[] = "Введите имя файла: ";
    write(STDOUT_FILENO, filename_prompt, sizeof(filename_prompt) - 1);
    
    char filename[256];
    ssize_t bytes = read(STDIN_FILENO, filename, sizeof(filename) - 1);
    if (bytes <= 0) {
        const char msg[] = "error: failed to read filename\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    
    if (bytes > 0 && filename[bytes - 1] == '\n') {
        filename[bytes - 1] = '\0';
    } else {
        filename[bytes] = '\0';
    }

    int parent_to_child[2];
    if (pipe(parent_to_child) == -1) {
        const char msg[] = "error: failed to create pipe\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    int child_to_parent[2];
    if (pipe(child_to_parent) == -1) {
        const char msg[] = "error: failed to create pipe\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    const pid_t child = fork();

    switch (child) {
    case -1: {
        const char msg[] = "error: failed to spawn new process\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    } break;

    case 0: {
        close(parent_to_child[1]);
        close(child_to_parent[0]);

        dup2(parent_to_child[0], STDIN_FILENO);
        close(parent_to_child[0]);

        dup2(child_to_parent[1], STDOUT_FILENO);
        close(child_to_parent[1]);

        char *const args[] = {CHILD_PROCESS_NAME, filename, NULL};
        execv(CHILD_PROCESS_NAME, args);

        const char msg[] = "error: failed to exec into child\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    } break;
    
    default: {
        close(parent_to_child[0]);
        close(child_to_parent[1]);

        {
            char msg[128];
            const int32_t length = snprintf(msg, sizeof(msg), "Дочерний процесс создан с PID: %d\n", child);
            write(STDOUT_FILENO, msg, length);
        }

        const char info[] = "Вводите числа через пробел (например: 100 2 5)\n";
        write(STDOUT_FILENO, info, sizeof(info) - 1);

        char buf[4096];
        int child_exited = 0;

        usleep(50000);

        while (!child_exited) {
            char response[256];
            ssize_t bytes = read(child_to_parent[0], response, sizeof(response));
            
            if (bytes > 0) {
                write(STDOUT_FILENO, response, bytes);
                
                if (strstr(response, "Processes terminating") != NULL) {
                    child_exited = 1;
                    break;
                }
            } 
            else if (bytes == 0) {
                child_exited = 1;
                break;
            }

            int status;
            pid_t result = waitpid(child, &status, WNOHANG);
            if (result != 0) {
                child_exited = 1;
                break;
            }

            const char prompt[] = "> ";
            write(STDOUT_FILENO, prompt, sizeof(prompt) - 1);

            bytes = read(STDIN_FILENO, buf, sizeof(buf));
            if (bytes < 0) {
                const char msg[] = "error: failed to read from stdin\n";
                write(STDERR_FILENO, msg, sizeof(msg) - 1);
                break;
            }
            else if (bytes == 0) {
                break;
            }

            ssize_t written = write(parent_to_child[1], buf, bytes);
            if (written == -1) {
                const char msg[] = "error: failed to write to child process\n";
                write(STDERR_FILENO, msg, sizeof(msg) - 1);
                break;
            }

            usleep(10000);
        }

        close(parent_to_child[1]);
        close(child_to_parent[0]);
        
        wait(NULL);

        const char msg[] = "Родительский процесс завершен\n";
        write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    } break;
    }

    return 0;
}