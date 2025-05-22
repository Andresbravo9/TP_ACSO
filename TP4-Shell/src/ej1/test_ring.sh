#!/bin/bash

# Colores para la salida
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Función para ejecutar una prueba
run_test() {
    local test_name=$1
    local n=$2      # número de procesos
    local c=$3      # valor inicial
    local s=$4      # proceso inicial
    local expected=$5  # valor esperado
    
    echo -e "\nPrueba: ${test_name}"
    echo "Parámetros: n=$n, c=$c, s=$s"
    echo "Valor esperado: $expected"
    
    # Ejecutar el programa y capturar la salida
    actual_output=$(./anillo $n $c $s)
    
    # Extraer el valor final de la salida
    final_value=$(echo "$actual_output" | grep "Valor final recibido:" | awk '{print $NF}')
    
    # Comparar la salida con lo esperado
    if [ "$final_value" == "$expected" ]; then
        echo -e "${GREEN}✓ Prueba exitosa${NC}"
    else
        echo -e "${RED}✗ Prueba fallida${NC}"
        echo "Valor esperado: $expected"
        echo "Valor obtenido: $final_value"
        echo "Salida completa:"
        echo "$actual_output"
    fi
}

# Función para validar argumentos inválidos
test_invalid_args() {
    local test_name=$1
    local args=$2
    local expected_error=$3
    
    echo -e "\nPrueba: ${test_name}"
    echo "Argumentos: $args"
    
    # Ejecutar el programa y capturar la salida de error
    actual_output=$(./anillo $args 2>&1)
    
    # Verificar si la salida contiene el mensaje de error esperado
    if echo "$actual_output" | grep -q "$expected_error"; then
        echo -e "${GREEN}✓ Prueba exitosa${NC}"
    else
        echo -e "${RED}✗ Prueba fallida${NC}"
        echo "Error esperado: $expected_error"
        echo "Salida obtenida: $actual_output"
    fi
}

# Compilar el programa
echo "Compilando anillo..."
gcc -o anillo ring.c
if [ $? -ne 0 ]; then
    echo -e "${RED}Error de compilación${NC}"
    exit 1
fi

# Tests de casos válidos
# En cada caso, el valor final debe ser el valor inicial + número de procesos
# porque cada proceso incrementa el valor en 1

# Test 1: Caso básico - 3 procesos
run_test "Caso básico - 3 procesos" 3 1 1 4

# Test 2: Caso básico - inicio desde proceso 2
run_test "Inicio desde proceso 2" 3 1 2 4

# Test 3: Caso con más procesos
run_test "Caso con 5 procesos" 5 1 1 6

# Test 4: Valor inicial grande
run_test "Valor inicial grande" 3 100 1 103

# Test 5: Máximo número de procesos razonable
run_test "Muchos procesos" 10 1 1 11

# Test 6: Inicio desde último proceso
run_test "Inicio desde último proceso" 4 1 4 5

# Tests de casos inválidos

# Test 7: Número incorrecto de argumentos
test_invalid_args "Argumentos insuficientes" "1 2" "Uso: anillo <n> <c> <s>"

# Test 8: Proceso inicial mayor que n
test_invalid_args "Proceso inicial inválido" "3 1 4" "Error: el proceso inicial debe estar entre 1 y n"

# Test 9: Proceso inicial negativo
test_invalid_args "Proceso inicial negativo" "3 1 -1" "Error: el proceso inicial debe estar entre 1 y n"

# Test 10: Número de procesos negativo
test_invalid_args "Número de procesos negativo" "-3 1 1" "Error: el número de procesos debe ser positivo"

echo -e "\nPruebas completadas." 