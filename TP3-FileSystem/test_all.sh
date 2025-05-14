#!/bin/bash

# Colores para mejor visualización
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

# Discos de prueba
DISKS=("basicDiskImage" "depthFileDiskImage" "dirFnameSizeDiskImage")
OPTIONS=("-i" "-p" "-ip")

echo -e "${BLUE}=== Iniciando pruebas exhaustivas ===${NC}"

run_test() {
    local disk=$1
    local option=$2
    
    echo -e "\n${BLUE}Probando $disk con opción $option${NC}"
    
    # Ejecutar nuestra implementación silenciosamente
    ./diskimageaccess $option samples/testdisks/$disk > our_output.txt 2>/dev/null
    
    # Si es la prueba completa (-ip), comparar con .gold
    if [ "$option" = "-ip" ]; then
        if diff our_output.txt samples/testdisks/${disk}.gold >/dev/null; then
            echo -e "${GREEN}✓ Prueba funcional exitosa${NC}"
        else
            echo -e "${RED}✗ Prueba funcional falló - Hay diferencias con archivo .gold${NC}"
            echo "¿Desea ver las diferencias? (s/n)"
            read -n 1 -r
            if [[ $REPLY =~ ^[Ss]$ ]]; then
                diff our_output.txt samples/testdisks/${disk}.gold
            fi
        fi
    fi

    # Prueba de memoria
    echo "Verificando fugas de memoria..."
    if valgrind --leak-check=full --error-exitcode=1 ./diskimageaccess $option samples/testdisks/$disk >/dev/null 2>valgrind.txt; then
        echo -e "${GREEN}✓ No hay fugas de memoria${NC}"
    else
        echo -e "${RED}✗ Se detectaron fugas de memoria${NC}"
        echo "¿Desea ver el reporte de Valgrind? (s/n)"
        read -n 1 -r
        if [[ $REPLY =~ ^[Ss]$ ]]; then
            cat valgrind.txt
        fi
    fi
}

# Compilar el proyecto
echo -e "${BLUE}Compilando el proyecto...${NC}"
make clean >/dev/null && make >/dev/null

# Ejecutar pruebas
for disk in "${DISKS[@]}"; do
    for option in "${OPTIONS[@]}"; do
        run_test "$disk" "$option"
    done
done

# Limpiar archivos temporales
rm -f our_output.txt valgrind.txt

echo -e "\n${BLUE}=== Pruebas completadas ===${NC}"
