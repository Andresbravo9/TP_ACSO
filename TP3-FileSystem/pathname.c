#include "pathname.h"
#include "directory.h"
#include "inode.h"
#include "diskimg.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>


int pathname_lookup(struct unixfilesystem *fs, const char *pathname) {
    if (!fs || !pathname) {
        return -1;
    }

    // Si la ruta está vacía o no comienza con '/'
    if (pathname[0] != '/' || strlen(pathname) == 0) {
        return -1;
    }

    // Empezamos desde el directorio raíz (inodo 1)
    int current_inode = ROOT_INUMBER;

    // Si la ruta es solo "/", retornamos el inodo raíz
    if (strlen(pathname) == 1) {
        return current_inode;
    }

    // Copiamos el pathname para poder modificarlo
    char path_copy[strlen(pathname) + 1];
    strcpy(path_copy, pathname);

    // Tokenizamos la ruta
    char *token = strtok(path_copy + 1, "/");  // +1 para saltar el primer '/'
    struct direntv6 dir_entry;

    while (token != NULL) {
        // Buscar el token en el directorio actual
        if (directory_findname(fs, token, current_inode, &dir_entry) != 0) {
            return -1;
        }

        // Actualizar el inodo actual
        current_inode = dir_entry.d_inumber;

        // Obtener el siguiente token
        token = strtok(NULL, "/");
    }

    return current_inode;
}
