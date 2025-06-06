#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

int main(int argc, char **argv) {
    if (argc != 4) {
        printf("Uso: anillo <n> <c> <s>\n");
        exit(1);
    }

    int n = atoi(argv[1]);  // número de procesos
    int c = atoi(argv[2]);  // valor inicial
    int s = atoi(argv[3]);  // proceso que inicia (0 a n-1)

    if (n <= 0) {
        printf("Error: el número de procesos debe ser positivo\n");
        exit(1);
    }
    if (s < 0 || s >= n) {
        printf("Error: el proceso inicial debe estar entre 0 y n-1\n");
        exit(1);
    }

    int ring_pipes[n][2];    // Pipes para la comunicación interna del anillo
    int start_pipe[2];       // Pipe del padre al proceso 's'
    int end_pipe[2];         // Pipe del proceso 's-1' al padre

    // Crear pipes del anillo
    for (int i = 0; i < n; i++) {
        if (pipe(ring_pipes[i]) == -1) {
            perror("Error creando ring_pipe");
            exit(1);
        }
    }
    // Crear pipes de comunicación con el padre
    if (pipe(start_pipe) == -1 || pipe(end_pipe) == -1) {
        perror("Error creando start_pipe o end_pipe");
        exit(1);
    }

    pid_t pids[n];

    // Crear procesos hijos
    for (int i = 0; i < n; i++) { // i es el índice del hijo (0 a n-1)
        pids[i] = fork();
        
        if (pids[i] == -1) {
            perror("Error en fork");
            exit(1);
        }
        
        if (pids[i] == 0) { // Proceso hijo
            int my_child_idx = i;
            int my_process_num = my_child_idx; 

            int read_from_fd;
            int write_to_fd;

            // Determinar de dónde leer y a dónde escribir
            if (my_process_num == s) {
                read_from_fd = start_pipe[0];
            } else {
                int prev_child_idx = (my_child_idx - 1 + n) % n;
                read_from_fd = ring_pipes[prev_child_idx][0];
            }

            int process_s_minus_1 = (s == 0) ? (n - 1) : (s - 1);
            if (my_process_num == process_s_minus_1) {
                write_to_fd = end_pipe[1];
            } else {
                write_to_fd = ring_pipes[my_child_idx][1];
            }

            close(start_pipe[1]); 
            if (start_pipe[0] != read_from_fd) close(start_pipe[0]);
            
            close(end_pipe[0]);  
            if (end_pipe[1] != write_to_fd) close(end_pipe[1]);

            for (int j = 0; j < n; j++) {
                if (ring_pipes[j][0] != read_from_fd) {
                    close(ring_pipes[j][0]);
                }
                if (ring_pipes[j][1] != write_to_fd) {
                    close(ring_pipes[j][1]);
                }
            }
            
            int valor;
            if (read(read_from_fd, &valor, sizeof(int)) == -1) {
                perror("Hijo: Error leyendo del pipe");
                exit(1);
            }
            close(read_from_fd); 

            // Si es el proceso inicial y recibe el valor inicial del padre
            if (my_process_num == s && valor == c) {
                printf("Proceso %d: Enviando valor inicial %d\n", my_process_num, valor + 1);
                valor++; 
            } else {
                printf("Proceso %d: Recibido %d, enviando %d\n", my_process_num, valor, valor + 1);
                valor++; 
            }
            fflush(stdout);

            if (write(write_to_fd, &valor, sizeof(int)) == -1) {
                perror("Hijo: Error escribiendo en el pipe");
                exit(1);
            }
            close(write_to_fd); 
            
            exit(0);
        }
    }

    // Proceso padre
    for (int i = 0; i < n; i++) {
        close(ring_pipes[i][0]);
        close(ring_pipes[i][1]);
    }

    close(start_pipe[0]); 
    close(end_pipe[1]);  

    // Enviar valor inicial al proceso 's'
    if (write(start_pipe[1], &c, sizeof(int)) == -1) {
        perror("Padre: Error enviando valor inicial");
        exit(1);
    }
    close(start_pipe[1]); 

    // Esperar valor final del proceso 's-1'
    int resultado;
    if (read(end_pipe[0], &resultado, sizeof(int)) == -1) {
        perror("Padre: Error recibiendo valor final");
        exit(1);
    }
    close(end_pipe[0]); 

    printf("Valor final después de dar la vuelta al anillo %d\n", resultado);

    // Esperar a que terminen todos los hijos
    for (int i = 0; i < n; i++) {
        waitpid(pids[i], NULL, 0);
    }

    return 0;
} 