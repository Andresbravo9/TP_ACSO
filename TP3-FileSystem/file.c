#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "file.h"
#include "inode.h"
#include "diskimg.h"

#ifndef SECTOR_SIZE
#define SECTOR_SIZE 512
#endif


int file_getblock(struct unixfilesystem *fs, int inumber, int blockNum, void *buf) {
    if (!fs || !buf || inumber < 1 || blockNum < 0) {
        return -1;
    }

    // Obtener el inodo
    struct inode inode;
    if (inode_iget(fs, inumber, &inode) != 0) {
        return -1;
    }

    // Obtener el tamaño del archivo
    int file_size = inode_getsize(&inode);
    if (file_size < 0) {
        return -1;
    }
    
    // Calcular el offset del bloque
    long block_offset = (long)blockNum * SECTOR_SIZE;
    
    // Si el offset está más allá del tamaño del archivo
    if (block_offset >= file_size) {
        return 0;
    }

    // Obtener el número de bloque real usando inode_indexlookup
    int real_block = inode_indexlookup(fs, &inode, blockNum);
    if (real_block <= 0) {
        return -1;
    }

    // Leer el bloque
    if (diskimg_readsector(fs->dfd, real_block, buf) != SECTOR_SIZE) {
        return -1;
    }

    // Calcular cuántos bytes son válidos en este bloque
    int valid_bytes;
    if (block_offset + SECTOR_SIZE > file_size) {
        valid_bytes = file_size - block_offset;
    } else {
        valid_bytes = SECTOR_SIZE;
    }

    return valid_bytes;
}

