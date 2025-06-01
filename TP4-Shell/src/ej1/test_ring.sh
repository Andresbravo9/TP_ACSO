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
    actual_output=$(./ring $n $c $s)
    
    # Extraer el valor final de la salida
    final_value=$(echo "$actual_output" | grep "Valor final después de dar la vuelta al anillo" | awk '{print $NF}')
    
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
    
    actual_output=$(./ring $args 2>&1)
    
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
gcc -o ring ring.c
if [ $? -ne 0 ]; then
    echo -e "${RED}Error de compilación${NC}"
    exit 1
fi

# Tests de casos válidos
# En cada caso, el valor final debe ser el valor inicial + número de procesos
# porque todos los procesos incrementan el valor en 1

# Test 1: Caso básico - 3 procesos (indexación 0-2)
run_test "Caso básico - 3 procesos" 3 1 0 4

# Test 2: Caso básico - inicio desde proceso 1 (indexación 0-2)
run_test "Inicio desde proceso 1" 3 1 1 4

# Test 3: Caso con más procesos (indexación 0-4)
run_test "Caso con 5 procesos" 5 1 0 6

# Test 4: Valor inicial grande
run_test "Valor inicial grande" 3 100 0 103

# Test 5: Máximo número de procesos razonable
run_test "Muchos procesos" 10 1 0 11

# Test 6: Inicio desde último proceso (indexación 0-3)
run_test "Inicio desde último proceso" 4 1 3 5

# Tests de casos inválidos

# Test 7: Número incorrecto de argumentos
test_invalid_args "Argumentos insuficientes" "1 2" "Uso: anillo <n> <c> <s>"

# Test 8: Proceso inicial mayor que n-1
test_invalid_args "Proceso inicial inválido" "3 1 3" "Error: el proceso inicial debe estar entre 0 y n-1"

# Test 9: Proceso inicial negativo
test_invalid_args "Proceso inicial negativo" "3 1 -1" "Error: el proceso inicial debe estar entre 0 y n-1"

# Test 10: Número de procesos negativo
test_invalid_args "Número de procesos negativo" "-3 1 1" "Error: el número de procesos debe ser positivo"

echo -e "\nPruebas completadas." 