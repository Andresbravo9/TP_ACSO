#!/bin/bash

# Colores para la salida
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Función para ejecutar una prueba
run_test() {
    local test_name=$1
    local command=$2
    local expected_output=$3
    local expect_error=${4:-false}
    
    echo -e "\nPrueba: ${test_name}"
    echo "Comando: ${command}"
    
    # Crear archivos temporales para la salida
    output_file=$(mktemp)
    error_file=$(mktemp)
    input_file=$(mktemp)
    
    # Preparar el archivo de entrada
    echo "${command}" > "$input_file"
    echo "exit" >> "$input_file"
    
    # Ejecutar el comando y capturar salidas
    ./shell < "$input_file" > "$output_file" 2> "$error_file"
    
    # Depuración
    echo "Salida completa:"
    cat "$output_file"
    
    # Verificar las condiciones de éxito de forma manual
    if [ "$expect_error" = true ]; then
        # Si esperamos un error
        if [ -s "$error_file" ] && grep -q "$expected_output" "$error_file"; then
            echo -e "${GREEN}✓ Prueba exitosa${NC}"
        else
            echo -e "${RED}✗ Prueba fallida${NC}"
            echo "Error esperado: $expected_output"
            echo "Error obtenido: $(cat $error_file)"
        fi
    else
        # Para pruebas normales, verificamos si la salida contiene lo esperado
        # Para los casos especiales de números, comprobamos directamente
        if [ "$expected_output" = "[0-9]" ]; then
            # Verificar si hay un número en la salida
            if grep -q '[0-9]' "$output_file"; then
                echo -e "${GREEN}✓ Prueba exitosa${NC}"
            else
                echo -e "${RED}✗ Prueba fallida${NC}"
                echo "Salida esperada: un número [0-9]"
                echo "Salida obtenida: $(cat $output_file)"
            fi
        elif grep -q "$expected_output" "$output_file"; then
            echo -e "${GREEN}✓ Prueba exitosa${NC}"
        else
            echo -e "${RED}✗ Prueba fallida${NC}"
            echo "Salida esperada: $expected_output"
            echo "Salida obtenida: $(cat $output_file)"
            if [ -s "$error_file" ]; then
                echo "Error obtenido: $(cat $error_file)"
            fi
        fi
    fi
    
    # Limpiar archivos temporales
    rm "$output_file" "$error_file" "$input_file"
}

# Compilar el shell
echo "Compilando shell..."
gcc -o shell shell.c
if [ $? -ne 0 ]; then
    echo -e "${RED}Error de compilación${NC}"
    exit 1
fi

# Tests de comandos básicos
run_test "Comando simple" \
         "ls" \
         "shell"

run_test "Comando con argumentos" \
         "ls -l" \
         "shell.c"

run_test "Comando vacío" \
         "" \
         ""

# Tests de pipes
run_test "Pipe simple" \
         "ls | grep shell" \
         "shell"

run_test "Pipe con espacios" \
         "ls    |    grep shell" \
         "shell"

# Tests específicos para pipes múltiples
run_test "Múltiples pipes" \
         "ls | grep .c | wc -l" \
         "[0-9]"

# Tests de errores
run_test "Comando inexistente" \
         "comandoquenoexiste" \
         "No such file or directory" \
         true

run_test "Pipe con comando inexistente" \
         "ls | comandoquenoexiste" \
         "No such file or directory" \
         true

run_test "Comando con error" \
         "ls archivoquenovaaestar" \
         "No existe el archivo o el directorio" \
         true

# Test de comandos más complejos
run_test "Comando con pipe y grep" \
         "ls -l | grep test" \
         "test.sh"

run_test "Conteo de archivos" \
         "ls | wc -l" \
         "[0-9]"

echo -e "\nPruebas completadas." 