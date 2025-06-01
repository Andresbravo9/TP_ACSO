#!/bin/bash

# Colores para la salida
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' 

# Función para ejecutar una prueba
run_test() {
    local test_name=$1
    local command=$2
    local expect_error=${3:-false}
    
    echo -e "\nPrueba: ${test_name}"
    echo "Comando: ${command}"
    
    # Crear archivos temporales para la salida
    custom_output=$(mktemp)
    custom_error=$(mktemp)
    custom_output_filtered=$(mktemp)
    bash_output=$(mktemp)
    bash_error=$(mktemp)
    input_file=$(mktemp)
    
    # Preparar el archivo de entrada para el shell personalizado
    echo "${command}" > "$input_file"
    echo "exit" >> "$input_file"
    
    # Ejecutar el comando en el shell personalizado
    ./shell < "$input_file" > "$custom_output" 2> "$custom_error"
    
    # Ejecutar el mismo comando en bash real
    bash -c "${command}" > "$bash_output" 2> "$bash_error" || true
    
    cat "$custom_output" | grep -v "Saliendo del shell" | sed 's/^Shell> //g' > "$custom_output_filtered"
    
    echo "Salida de tu shell (original):"
    cat "$custom_output"
    echo "Salida de tu shell (filtrada):"
    cat "$custom_output_filtered"
    echo "Salida de bash real:"
    cat "$bash_output"
    
    # Verificar si las salidas coinciden
    if [ "$expect_error" = true ]; then
        if [ -s "$custom_error" ] && [ -s "$bash_error" ]; then
            echo -e "${GREEN}✓ Prueba exitosa - Ambos shells reportaron errores${NC}"
            echo "Error en bash:"
            cat "$bash_error"
            echo "Error en tu shell:"
            cat "$custom_error"
        else
            echo -e "${RED}✗ Prueba fallida - Comportamiento diferente con errores${NC}"
            echo "Error en bash:"
            cat "$bash_error"
            echo "Error en tu shell:"
            cat "$custom_error"
        fi
    else
        if diff -q "$custom_output_filtered" "$bash_output" >/dev/null; then
            echo -e "${GREEN}✓ Prueba exitosa - Las salidas coinciden exactamente${NC}"
        else
            # Si no coinciden exactamente, verificar si contienen información similar
            if grep -q "$(cat $bash_output | tr -d '\n')" "$custom_output_filtered" 2>/dev/null; then
                echo -e "${GREEN}✓ Prueba exitosa - Las salidas contienen información similar${NC}"
            else
                echo -e "${RED}✗ Prueba fallida - Las salidas son diferentes${NC}"
                echo "Salida esperada (bash):"
                cat "$bash_output"
                echo "Salida obtenida (tu shell, filtrada):"
                cat "$custom_output_filtered"
            fi
        fi
    fi
    
    # Limpiar archivos temporales
    rm "$custom_output" "$custom_error" "$bash_output" "$bash_error" "$input_file" "$custom_output_filtered"
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
         "ls"

run_test "Comando con argumentos" \
         "ls -l"

run_test "Comando vacío" \
         ""

# Tests de pipes
run_test "Pipe simple" \
         "ls | grep shell"

run_test "Pipe con espacios" \
         "ls    |    grep shell"

# Tests específicos para pipes múltiples
run_test "Múltiples pipes" \
         "ls | grep .c | wc -l"

# Tests de errores
run_test "Comando inexistente" \
         "comandoquenoexiste" \
         true

run_test "Pipe con comando inexistente" \
         "ls | comandoquenoexiste" \
         true

run_test "Comando con error" \
         "ls archivoquenovaaestar" \
         true

# Test de comandos más complejos
run_test "Comando con pipe y grep" \
         "ls -l | grep test"

run_test "Conteo de archivos" \
         "ls | wc -l"

echo -e "\nPruebas completadas." 