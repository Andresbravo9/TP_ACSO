#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>

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

// Estructura para almacenar información de redirección
typedef struct {
    char *input_file;     // Archivo de entrada (<)
    char *output_file;    // Archivo de salida (>)
    int append_output;    // Bandera para modo append (>>)
} RedirectionInfo;

// Función para analizar comando y detectar redirecciones
void parse_command(char *original_command_input_str, char **args, int child_idx_debug, RedirectionInfo *redirection) {
    if (original_command_input_str == NULL) {
        args[0] = NULL;
        return;
    }

    // Trabajar con una copia del comando original
    char *command_copy = strdup(original_command_input_str);
    if (!command_copy) {
        perror("Error al duplicar comando");
        args[0] = NULL;
        return;
    }

    int i = 0;
    int redirect_in = 0;
    int redirect_out = 0;
    char *token, *saveptr;
    
    // Primera pasada: buscar redirecciones
    token = strtok_r(command_copy, " \t\n", &saveptr);
    while (token != NULL && i < MAX_ARGS - 1) {
        if (strcmp(token, "<") == 0) {
            redirect_in = 1;
            token = strtok_r(NULL, " \t\n", &saveptr);
            if (token) {
                redirection->input_file = strdup(token);
            }
        } else if (strcmp(token, ">") == 0) {
            redirect_out = 1;
            token = strtok_r(NULL, " \t\n", &saveptr);
            if (token) {
                redirection->output_file = strdup(token);
                redirection->append_output = 0;
            }
        } else if (strcmp(token, ">>") == 0) {
            redirect_out = 1;
            token = strtok_r(NULL, " \t\n", &saveptr);
            if (token) {
                redirection->output_file = strdup(token);
                redirection->append_output = 1;
            }
        } else if (!redirect_in && !redirect_out) {
            // Si no hemos encontrado redirecciones aún, añadir como argumento
            args[i++] = strdup(token);
        }
        
        // Si ya estamos en modo redirección, solo procesamos los operadores y sus argumentos
        if (redirect_in || redirect_out) {
            redirect_in = redirect_out = 0;  // Resetear para el siguiente token
        }
        
        token = strtok_r(NULL, " \t\n", &saveptr);
    }
    
    args[i] = NULL;
    free(command_copy);
}

// Función para ejecutar comandos internos (cd)
int execute_internal_command(char **args) {
    if (!args || !args[0]) return 0;
    
    // Comando cd
    if (strcmp(args[0], "cd") == 0) {
        if (!args[1]) {
            // Si no hay argumentos, ir al directorio HOME
            if (chdir(getenv("HOME")) != 0) {
                perror("cd HOME");
            }
        } else {
            if (chdir(args[1]) != 0) {
                perror("cd");
            }
        }
        return 1; // Comando interno ejecutado
    }
    
    return 0; // No es un comando interno
}

// Libera memoria utilizada por la estructura de redirección
void free_redirection(RedirectionInfo *redirection) {
    if (!redirection) return;
    if (redirection->input_file) free(redirection->input_file);
    if (redirection->output_file) free(redirection->output_file);
    
    // Resetear la estructura
    redirection->input_file = NULL;
    redirection->output_file = NULL;
    redirection->append_output = 0;
}

// Libera memoria utilizada por los argumentos
void free_args(char **args) {
    if (!args) return;
    for (int i = 0; args[i] != NULL; i++) {
        free(args[i]);
        args[i] = NULL;
    }
}

// Función para configurar redirecciones
void setup_redirections(RedirectionInfo *redirection) {
    if (!redirection) return;
    
    // Redirección de entrada
    if (redirection->input_file) {
        int fd = open(redirection->input_file, O_RDONLY);
        if (fd == -1) {
            perror("Error al abrir archivo de entrada");
            return;
        }
        if (dup2(fd, STDIN_FILENO) == -1) {
            perror("Error en dup2 para entrada");
            close(fd);
            return;
        }
        close(fd);
    }
    
    // Redirección de salida
    if (redirection->output_file) {
        int flags = O_WRONLY | O_CREAT;
        if (redirection->append_output) {
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }
        
        int fd = open(redirection->output_file, flags, 0644);
        if (fd == -1) {
            perror("Error al abrir archivo de salida");
            return;
        }
        if (dup2(fd, STDOUT_FILENO) == -1) {
            perror("Error en dup2 para salida");
            close(fd);
            return;
        }
        close(fd);
    }
}

int execute_command(char **args, RedirectionInfo *redirection) {
    if (!args || !args[0]) return 0;
    
    // Verificar si es un comando interno
    if (execute_internal_command(args)) {
        return 0;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        perror("Error en fork");
        return -1;
    }

    if (pid == 0) { // Proceso hijo
        // Configurar redirecciones
        setup_redirections(redirection);
        
        // Ejecutar el comando
        execvp(args[0], args);
        perror("Error en execvp");
        exit(EXIT_FAILURE);
    } else {
        // Proceso padre
        int status;
        waitpid(pid, &status, 0);
    }
    return 0;
}

int execute_pipeline(char **commands, int num_commands) {
    if (num_commands <= 0) return -1;
    
    // Caso especial: un solo comando
    if (num_commands == 1) {
        char *args[MAX_ARGS];
        RedirectionInfo redirection = {NULL, NULL, 0};
        
        // Parsear comando y detectar redirecciones
        parse_command(commands[0], args, 0, &redirection);
        if (!args[0]) {
            free_redirection(&redirection);
            return -1;
        }
        
        // Ejecutar comando (interno o externo)
        int result;
        if (execute_internal_command(args)) {
            result = 0;
        } else {
            result = execute_command(args, &redirection);
        }
        
        free_args(args);
        free_redirection(&redirection);
        return result;
    }

    // Múltiples comandos con pipes
    int pipes[num_commands - 1][2];
    pid_t pids[num_commands];

    // Crear todos los pipes necesarios
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("Error creando pipe");
            for (int j = 0; j < i; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            return -1;
        }
    }

    // Crear procesos para cada comando
    for (int i = 0; i < num_commands; i++) {
        pids[i] = fork();
        
        if (pids[i] == -1) {
            perror("Error en fork");
            // Limpiar recursos
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            for (int j = 0; j < i; j++) {
                kill(pids[j], SIGTERM);
                waitpid(pids[j], NULL, 0);
            }
            return -1;
        }

        if (pids[i] == 0) { // Proceso hijo
            // Configurar redirecciones de pipes
            if (i > 0) {
                if (dup2(pipes[i-1][0], STDIN_FILENO) == -1) {
                    perror("Error en dup2 (stdin)");
                    exit(EXIT_FAILURE);
                }
            }
            
            if (i < num_commands - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                    perror("Error en dup2 (stdout)");
                    exit(EXIT_FAILURE);
                }
            }
            
            // Cerrar todos los pipes en el hijo
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            // Parsear y configurar redirecciones para este comando
            char *args[MAX_ARGS];
            RedirectionInfo redirection = {NULL, NULL, 0};
            
            parse_command(commands[i], args, i, &redirection);
            if (!args[0]) {
                free_redirection(&redirection);
                exit(EXIT_FAILURE);
            }
            
            // Solo aplicar redirección de entrada para el primer comando
            // y redirección de salida para el último comando
            if (i == 0 && redirection.input_file) {
                int fd = open(redirection.input_file, O_RDONLY);
                if (fd != -1) {
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                } else {
                    perror("Error al abrir archivo de entrada");
                }
            }
            
            if (i == num_commands - 1 && redirection.output_file) {
                int flags = O_WRONLY | O_CREAT;
                flags |= redirection.append_output ? O_APPEND : O_TRUNC;
                
                int fd = open(redirection.output_file, flags, 0644);
                if (fd != -1) {
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                } else {
                    perror("Error al abrir archivo de salida");
                }
            }
            
            // Ejecutar comando
            execvp(args[0], args);
            perror("Error en execvp");
            
            // Liberar recursos en caso de error
            free_args(args);
            free_redirection(&redirection);
            exit(EXIT_FAILURE);
        }
    }

    // Proceso padre: cerrar todos los pipes
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Esperar a que terminen todos los hijos
    for (int i = 0; i < num_commands; i++) {
        waitpid(pids[i], NULL, 0);
    }
    
    return 0;
}

int main() {
    char command_input[COMMAND_LENGTH];
    
    while (1) {
        printf("Shell> ");
        fflush(stdout);

        if (fgets(command_input, sizeof(command_input), stdin) == NULL) {
            printf("\nSaliendo del shell...\n");
            break;
        }
        
        // Eliminar salto de línea
        command_input[strcspn(command_input, "\n")] = '\0';
        
        // Eliminar espacios al principio y al final
        char *trimmed_input = trim(command_input);
        if (!trimmed_input || strlen(trimmed_input) == 0) {
            continue;
        }
        
        // Comprobar si es el comando de salida
        if (strcmp(trimmed_input, "exit") == 0) {
            printf("Saliendo del shell...\n");
            break;
        }
        
        // Copiar el comando para no modificar el original
        char *command_copy = strdup(trimmed_input);
        if (!command_copy) {
            perror("Error al copiar comando");
            continue;
        }
        
        // Dividir por pipes
        char *commands[MAX_COMMANDS + 1]; 
        int command_count = 0;
        
        char *token, *saveptr;
        token = strtok_r(command_copy, "|", &saveptr);
        while (token && command_count < MAX_COMMANDS) {
            char *trimmed_token = trim(token);
            if (trimmed_token && strlen(trimmed_token) > 0) {
                commands[command_count] = strdup(trimmed_token);
                if (!commands[command_count]) {
                    perror("Error al duplicar comando");
                    break;
                }
                command_count++;
            }
            token = strtok_r(NULL, "|", &saveptr);
        }
        commands[command_count] = NULL;
        free(command_copy);
        
        if (command_count == 0) {
            continue;
        }
        
        // Ejecutar el pipeline de comandos
        execute_pipeline(commands, command_count);
        
        // Liberar memoria
        for (int i = 0; i < command_count; i++) {
            free(commands[i]);
        }
    }
    
    return 0;
}