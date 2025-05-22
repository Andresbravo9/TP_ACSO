#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#define MAX_COMMANDS 200
#define MAX_ARGS 64
#define COMMAND_LENGTH 256

char* trim(char* str) {
    if (str == NULL) return NULL;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;

    char* end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void parse_command(char *original_command_input_str, char **args, int child_idx_debug) {
    char command_input_copy[COMMAND_LENGTH];

    if (original_command_input_str == NULL) {
        args[0] = NULL;
        return;
    }

    strncpy(command_input_copy, original_command_input_str, COMMAND_LENGTH -1);
    command_input_copy[COMMAND_LENGTH-1] = '\0';

    int i = 0;
    char *saveptr;
    char *token = strtok_r(command_input_copy, " \t\n\r", &saveptr);
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok_r(NULL, " \t\n\r", &saveptr);
    }
    args[i] = NULL;
}

int execute_command(char **args) {
    if (args == NULL || args[0] == NULL) {
        return 0;
    }
    pid_t pid = fork();

    if (pid == -1) {
        perror("Error en fork");
        return -1;
    }

    if (pid == 0) {
        execvp(args[0], args);
        perror("Error en execvp");
        exit(EXIT_FAILURE);
    } else {
        int status;
        waitpid(pid, &status, 0);
    }
    return 0;
}

int execute_pipeline(char **commands, int num_commands) {
    if (num_commands <= 0) {
        return -1;
    }
    if (num_commands == 1) {
         char *args_single[MAX_ARGS];
         char temp_single_cmd[COMMAND_LENGTH];
            if (commands[0] != NULL) {
                strncpy(temp_single_cmd, commands[0], COMMAND_LENGTH - 1);
                temp_single_cmd[COMMAND_LENGTH - 1] = '\0';
            } else {
                return -1;
            }
         parse_command(temp_single_cmd, args_single, 0); 
         if (args_single[0] == NULL) {
             return -1;
        }
        return execute_command(args_single);
    }

    int pipes_fd[num_commands - 1][2];
    pid_t pids[num_commands];

    // Crear todos los pipes necesarios
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipes_fd[i]) == -1) {
            perror("Error creando pipe en execute_pipeline");
            // Limpieza de pipes creados antes del fallo
            for(int k=0; k<i; ++k) {
                close(pipes_fd[k][0]);
                close(pipes_fd[k][1]);
            }
            return -1;
        }
    }

    // Crear los procesos hijos
    for (int i = 0; i < num_commands; i++) {
        pids[i] = fork();

        if (pids[i] == -1) {
            perror("Error en fork en execute_pipeline");
            // Limpieza de pipes
            for(int k_pipe=0; k_pipe < num_commands - 1; ++k_pipe) {
                close(pipes_fd[k_pipe][0]);
                close(pipes_fd[k_pipe][1]);
            }
            // Esperar a los hijos que sí se crearon para evitar zombies
            for(int k_child=0; k_child < i; ++k_child) {
                waitpid(pids[k_child], NULL, 0);
            }
            return -1; 
        }

        if (pids[i] == 0) { // Proceso Hijo i
            char *args_child[MAX_ARGS];

            // Configurar redirección de entrada para todos excepto el primer proceso
            if (i > 0) { 
                if (dup2(pipes_fd[i-1][0], STDIN_FILENO) == -1) { 
                    perror("Error en dup2 (entrada)"); 
                    exit(EXIT_FAILURE); 
                }
            }
            
            // Configurar redirección de salida para todos excepto el último proceso
            if (i < num_commands - 1) { 
                if (dup2(pipes_fd[i][1], STDOUT_FILENO) == -1) { 
                    perror("Error en dup2 (salida)"); 
                    exit(EXIT_FAILURE); 
                }
            }

            // Cerrar todos los descriptores de archivo de pipes que este proceso no necesita
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes_fd[j][0]);
                close(pipes_fd[j][1]);
            }

            // Parsear y ejecutar el comando
            parse_command(commands[i], args_child, i); 

            if (args_child[0] == NULL) {
                exit(EXIT_FAILURE);
            }

            execvp(args_child[0], args_child);
            perror("Error en execvp");
            exit(EXIT_FAILURE);
        }
    }

    // Proceso Padre - Cerrar todos los descriptores de pipes
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipes_fd[i][0]);
        close(pipes_fd[i][1]);
    }

    // Esperar que todos los hijos terminen
    for (int i = 0; i < num_commands; i++) {
        int status;
        waitpid(pids[i], &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            // Un hijo terminó con error, pero seguimos esperando a los demás
            continue;
        }
    }
    
    return 0;
}

int main() {
    char command_input[COMMAND_LENGTH];
    char command_copy[COMMAND_LENGTH];
    char *commands[MAX_COMMANDS + 1]; 

    while (1) {
        printf("Shell> ");
        fflush(stdout);

        if (fgets(command_input, sizeof(command_input), stdin) == NULL) {
            printf("\nSaliendo del shell...\n");
            break;
        }
        command_input[strcspn(command_input, "\n")] = '\0';

        char *trimmed_input = trim(command_input);
        if (trimmed_input == NULL || strlen(trimmed_input) == 0) {
            continue;
        }
        
        strncpy(command_copy, trimmed_input, COMMAND_LENGTH -1);
        command_copy[COMMAND_LENGTH -1] = '\0';

        if (strcmp(command_copy, "exit") == 0) {
            printf("Saliendo del shell...\n");
            break;
        }

        int command_count = 0;
        char *saveptr_main;
        char *token = strtok_r(command_copy, "|", &saveptr_main);
        while (token != NULL && command_count < MAX_COMMANDS) {
            char *trimmed_token_ptr = trim(token);
            if (trimmed_token_ptr != NULL && strlen(trimmed_token_ptr) > 0) {
                 commands[command_count] = strdup(trimmed_token_ptr);
                 if (commands[command_count] == NULL) {
                     perror("Error en strdup en main");
                     for (int k_free = 0; k_free < command_count; ++k_free) free(commands[k_free]);
                     command_count = 0; 
                     break; 
                 }
                 command_count++;
            }
            token = strtok_r(NULL, "|", &saveptr_main);
        }
        commands[command_count] = NULL; 
        
        if (command_count == 0) {
            continue;
        }

        if (command_count == 1) {
            char *args_single[MAX_ARGS];
            char temp_single_cmd[COMMAND_LENGTH]; 

            strncpy(temp_single_cmd, commands[0], COMMAND_LENGTH - 1);
            temp_single_cmd[COMMAND_LENGTH - 1] = '\0';
            
            parse_command(temp_single_cmd, args_single, -1); // -1 para comando simple en main
            
            if (args_single[0] == NULL) {
                continue;
            }

            if (strcmp(args_single[0], "exit") == 0) {
                for (int k_free = 0; k_free < command_count; ++k_free) {
                    free(commands[k_free]);
                }
                printf("Saliendo del shell...\n");
                break;
            }
            
            execute_command(args_single);
        } else {
            execute_pipeline(commands, command_count);
        }

        // Liberar memoria de los comandos
        for (int k_free = 0; k_free < command_count; ++k_free) {
            free(commands[k_free]);
        }
    }
    return 0;
}