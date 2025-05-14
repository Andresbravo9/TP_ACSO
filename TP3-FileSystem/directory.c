#include "directory.h"
#include "inode.h"
#include "diskimg.h"
#include "file.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifndef SECTOR_SIZE
#define SECTOR_SIZE 512
#endif


int directory_findname(struct unixfilesystem *fs, const char *name,
		int dirinumber, struct direntv6 *dirEnt) {
    if (!fs || !name || !dirEnt || dirinumber < 1 || strlen(name) == 0 || strlen(name) > 14) {
        return -1;
    }

    // Obtener el inodo del directorio
    struct inode dir_inode;
    if (inode_iget(fs, dirinumber, &dir_inode) != 0) {
        return -1;
    }

    // Verificar que es un directorio
    if ((dir_inode.i_mode & IFMT) != IFDIR) {
        return -1;
    }

    // Calcular el número de entradas en el directorio
    int dir_size = inode_getsize(&dir_inode);
    if (dir_size < 0 || dir_size % sizeof(struct direntv6) != 0) {
        return -1;
    }
    
    int num_entries = dir_size / sizeof(struct direntv6);
    if (num_entries == 0) {
        return -1;
    }

    // Buffer para leer un bloque
    unsigned char block[SECTOR_SIZE];
    struct direntv6 *entries = (struct direntv6 *)block;
    
    // Iterar sobre los bloques del directorio
    int current_block = 0;
    int entries_processed = 0;

    while (entries_processed < num_entries) {
        // Leer un bloque del directorio
        int bytes_read = file_getblock(fs, dirinumber, current_block, block);
        if (bytes_read <= 0) {
            return -1;
        }

        // Número de entradas en este bloque
        int entries_in_block = bytes_read / sizeof(struct direntv6);

        // Buscar en las entradas de este bloque
        for (int i = 0; i < entries_in_block && entries_processed < num_entries; i++) {
            if (entries[i].d_inumber != 0 && strncmp(entries[i].d_name, name, 14) == 0) {
                // Encontramos la entrada y verificamos que el inodo existe
                struct inode test_inode;
                if (inode_iget(fs, entries[i].d_inumber, &test_inode) == 0) {
                    memcpy(dirEnt, &entries[i], sizeof(struct direntv6));
                    return 0;
                }
            }
            entries_processed++;
        }

        current_block++;
    }

    return -1;  // No se encontró la entrada
}
