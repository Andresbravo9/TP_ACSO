#!/bin/bash

# Colores para la salida
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Función para ejecutar una prueba
run_test() {
    local diskname=$1
    echo -e "\n${GREEN}=== Probando $diskname ===${NC}"
    
    # Ejecutar la prueba normal
    ./diskimageaccess -ip samples/testdisks/$diskname > output_$diskname.txt
    
    # Comparar con el archivo .gold
    if diff output_$diskname.txt samples/testdisks/$diskname.gold > diff_$diskname.txt; then
        echo -e "${GREEN}✓ Prueba funcional exitosa${NC}"
        rm diff_$diskname.txt
    else
        echo -e "${RED}✗ Prueba funcional falló${NC}"
        echo "Diferencias encontradas:"
        cat diff_$diskname.txt
    fi

    # Ejecutar prueba de memoria con Valgrind
    echo -e "\n${GREEN}=== Prueba de memoria para $diskname ===${NC}"
    valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
             --error-exitcode=1 ./diskimageaccess -ip samples/testdisks/$diskname > /dev/null 2>valgrind_$diskname.txt
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ No se encontraron fugas de memoria${NC}"
        rm valgrind_$diskname.txt
    else
        echo -e "${RED}✗ Se encontraron problemas de memoria${NC}"
        echo "Detalles del análisis de memoria:"
        cat valgrind_$diskname.txt
    fi
    echo -e "${GREEN}=== Fin de pruebas para $diskname ===${NC}\n"
}

# Compilar el proyecto
echo -e "${GREEN}=== Compilando... ===${NC}"
make clean && make

# Ejecutar las tres pruebas
run_test basicDiskImage
run_test depthFileDiskImage
run_test dirFnameSizeDiskImage

# Limpiar archivos temporales
rm -f output_*.txt