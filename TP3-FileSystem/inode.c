#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "inode.h"
#include "diskimg.h"
#include "unixfilesystem.h"

#ifndef SECTOR_SIZE
#define SECTOR_SIZE 512
#endif


int inode_iget(struct unixfilesystem *fs, int inumber, struct inode *inp) {
    if (!fs || !inp || inumber < 1) {
        return -1;
    }

    // Calcular el número total de inodos en el sistema de archivos
    int inodes_per_sector = SECTOR_SIZE / sizeof(struct inode);
    int total_inodes = fs->superblock.s_isize * inodes_per_sector;

    // Verificar que el número de inodo es válido
    if (inumber > total_inodes) {
        return -1;
    }

    // Calcular el sector donde está el inodo
    int sector = INODE_START_SECTOR + (inumber - 1) / inodes_per_sector;
    
    // Verificar que el sector está dentro de los límites del sistema de archivos
    if (sector >= INODE_START_SECTOR + fs->superblock.s_isize) {
        return -1;
    }

    // Leer el sector completo
    unsigned char sector_data[SECTOR_SIZE];
    if (diskimg_readsector(fs->dfd, sector, sector_data) != SECTOR_SIZE) {
        return -1;
    }

    // Calcular el offset dentro del sector
    int offset = ((inumber - 1) % inodes_per_sector) * sizeof(struct inode);
    if (offset + sizeof(struct inode) > SECTOR_SIZE) {
        return -1;
    }
    
    // Copiar el inodo desde el sector a la estructura proporcionada
    memcpy(inp, &sector_data[offset], sizeof(struct inode));

    // Verificar si el inodo está asignado
    if ((inp->i_mode & IALLOC) == 0) {
        return -1;
    }

    return 0;
}


int inode_indexlookup(struct unixfilesystem *fs, struct inode *inp, int blockNum) {  
    if (!fs || !inp || blockNum < 0) {
        return -1;
    }

    // Verificar si el archivo usa el esquema de archivo grande
    if (inp->i_mode & ILARG) {
        // Esquema de archivo grande 
        if (blockNum >= 7 * 256 + 256 * 256) { // Más allá del tamaño máximo posible
            return -1;
        }

        if (blockNum < 7 * 256) {
            // Bloques indirectos simples 
            int indirect_block = blockNum / 256;
            int indirect_offset = blockNum % 256;
            
            // Verificar que el bloque indirecto es válido
            if (inp->i_addr[indirect_block] == 0) {
                return -1;
            }

            // Leer el bloque indirecto
            unsigned char indirect_data[SECTOR_SIZE];
            if (diskimg_readsector(fs->dfd, inp->i_addr[indirect_block], indirect_data) != SECTOR_SIZE) {
                return -1;
            }
            
            uint16_t *block_numbers = (uint16_t *)indirect_data;
            if (block_numbers[indirect_offset] == 0) {
                return -1;
            }
            return block_numbers[indirect_offset];
        } else {
            // Bloque doblemente indirecto 
            blockNum -= 7 * 256;
            int double_indirect = blockNum / 256;
            int indirect_offset = blockNum % 256;
            
            // Verificar que el bloque doblemente indirecto es válido
            if (inp->i_addr[7] == 0) {
                return -1;
            }

            // Leer el bloque doblemente indirecto
            unsigned char double_indirect_data[SECTOR_SIZE];
            if (diskimg_readsector(fs->dfd, inp->i_addr[7], double_indirect_data) != SECTOR_SIZE) {
                return -1;
            }
            
            uint16_t *indirect_blocks = (uint16_t *)double_indirect_data;
            if (indirect_blocks[double_indirect] == 0) {
                return -1;
            }

            // Leer el bloque indirecto
            unsigned char indirect_data[SECTOR_SIZE];
            if (diskimg_readsector(fs->dfd, indirect_blocks[double_indirect], indirect_data) != SECTOR_SIZE) {
                return -1;
            }
            
            uint16_t *block_numbers = (uint16_t *)indirect_data;
            if (block_numbers[indirect_offset] == 0) {
                return -1;
            }
            return block_numbers[indirect_offset];
        }
    } else {
        // Esquema de archivo pequeño 
        if (blockNum >= 8) {
            return -1;
        }
        // Verificar que el bloque directo es válido
        if (inp->i_addr[blockNum] == 0) {
            return -1;
        }
        return inp->i_addr[blockNum];
    }
}

int inode_getsize(struct inode *inp) {
    if (!inp) {
        return -1;
    }
    return ((inp->i_size0 << 16) | inp->i_size1);
}
