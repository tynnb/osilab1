#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        const char msg[] = "Usage: child <filename>\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    int32_t file = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (file == -1) {
        const char msg[] = "error: failed to open file\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    const char header[] = "==Results of division==\n";
    write(file, header, sizeof(header) - 1);

    {
        char msg[128];
        int len = snprintf(msg, sizeof(msg), "Child process started. File: %s\n", argv[1]);
        write(STDOUT_FILENO, msg, len);
    }

    char buf[4096];
    ssize_t bytes;

    while ((bytes = read(STDIN_FILENO, buf, sizeof(buf) - 1)) > 0) {
        buf[bytes] = '\0';

        int *numbers = NULL;
        int capacity = 10;
        int count = 0;
        
        numbers = malloc(capacity * sizeof(int));
        if (numbers == NULL) {
            const char msg[] = "error: memory allocation failed\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }

        int i = 0;
        while (i < bytes) {
            while (i < bytes && (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\n' || buf[i] == '\r')) {
                i++;
            }
            
            if (i >= bytes) {
                break;
            }

            char *endptr;
            long num = strtol(&buf[i], &endptr, 10);
            
            if (endptr != &buf[i]) {
                if (count >= capacity) {
                    capacity *= 2;
                    int *new_numbers = realloc(numbers, capacity * sizeof(int));
                    if (new_numbers == NULL) {
                        const char msg[] = "error: memory reallocation failed\n";
                        write(STDERR_FILENO, msg, sizeof(msg) - 1);
                        free(numbers);
                        exit(EXIT_FAILURE);
                    }
                    numbers = new_numbers;
                }
                
                numbers[count++] = (int)num;
                i = endptr - buf;
            } else {
                i++;
            }
        }

        if (count < 2) {
            const char msg[] = "error: need at least 2 numbers\n";
            write(STDOUT_FILENO, msg, sizeof(msg) - 1);
            free(numbers);
            continue;
        }

        int op_capacity = 256;
        char *operation = malloc(op_capacity);
        if (operation == NULL) {
            const char msg[] = "error: memory allocation failed\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            free(numbers);
            exit(EXIT_FAILURE);
        }
        
        int op_len = snprintf(operation, op_capacity, "Operation: %d", numbers[0]);
        double result = (double)numbers[0];

        for (int i = 1; i < count; i++) {
            if (numbers[i] == 0) {
                int needed = snprintf(NULL, 0, " / %d", numbers[i]);
                if (op_len + needed >= op_capacity) {
                    op_capacity = op_len + needed + 64;
                    char *new_operation = realloc(operation, op_capacity);
                    if (new_operation == NULL) {
                        const char msg[] = "error: memory reallocation failed\n";
                        write(STDERR_FILENO, msg, sizeof(msg) - 1);
                        free(operation);
                        free(numbers);
                        exit(EXIT_FAILURE);
                    }
                    operation = new_operation;
                }
                op_len += snprintf(operation + op_len, op_capacity - op_len, " / %d", numbers[i]);

                write(file, operation, op_len);
                const char error_msg[] = " = error: division by zero\n";
                write(file, error_msg, sizeof(error_msg) - 1);

                const char response[] = "error: division by zero. Processes terminating\n";
                write(STDOUT_FILENO, response, sizeof(response) - 1);

                free(operation);
                free(numbers);
                close(file);
                exit(EXIT_FAILURE);
            }
            
            int needed = snprintf(NULL, 0, " / %d", numbers[i]);
            if (op_len + needed >= op_capacity) {
                op_capacity = op_len + needed + 64;
                char *new_operation = realloc(operation, op_capacity);
                if (new_operation == NULL) {
                    const char msg[] = "error: memory reallocation failed\n";
                    write(STDERR_FILENO, msg, sizeof(msg) - 1);
                    free(operation);
                    free(numbers);
                    exit(EXIT_FAILURE);
                }
                operation = new_operation;
            }
            op_len += snprintf(operation + op_len, op_capacity - op_len, " / %d", numbers[i]);
            
            result /= numbers[i];
        }

        char result_str[64];
        snprintf(result_str, sizeof(result_str), " = %.6f\n", result);
        
        write(file, operation, op_len);
        write(file, result_str, strlen(result_str));

        char response[128];
        snprintf(response, sizeof(response), "result: %.6f (written to file)\n", result);
        write(STDOUT_FILENO, response, strlen(response));
        
        fsync(STDOUT_FILENO);

        free(operation);
        free(numbers);
    }

    close(file);
    return 0;
}