#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "mdfu/image_reader.h"

static FILE *image = NULL;

/**
 * @brief Opens an image file in binary read mode.
 *
 * This function attempts to open an image file whose path is provided by the
 * caller. The file is opened in binary read mode. If the file cannot be opened,
 * the function returns an error code.
 *
 * @param fpath A pointer to a null-terminated string that specifies the path to the image file.
 * @return int Returns 0 if the file is successfully opened, or -1 if the file cannot be opened
 * with `errno` set appropriately.
 */
static int open(const char *fpath){
    image = fopen(fpath, "rb");
    if(image == NULL){
        return -1;
    }
    return 0;
}

/**
 * @brief Closes an open file.
 *
 * This function attempts to close a file that is associated with the global
 * file pointer 'image'. If 'image' is NULL, indicating that there is no open
 * file, it sets 'errno' to EBADF and returns -1. If 'fclose' fails for any
 * other reason, 'image' is set to NULL and the function also returns -1.
 * On success, 'image' is set to NULL and the function returns 0.
 *
 * @return int Returns 0 on success, -1 on error with 'errno' set appropriately.
 */
static int close(){
    if(NULL == image){
        errno = EBADF;
        return -1;
    }
    if(fclose(image) == EOF){
        image = NULL;
        return -1;
    }
    image = NULL;
    return 0;
}

/**
 * @brief Reads data from a file stream into a given buffer.
 *
 * This function attempts to read up to `size` bytes from the file stream
 * pointed to by the global variable `image` into the buffer pointed to by `data`.
 *
 * @param data A pointer to the buffer where the read bytes will be stored.
 * @param size The number of bytes to read.
 * @return On success, the number of bytes read is returned (zero indicates end of file).
 *         On error, -1 is returned, and `errno` is set appropriately.
 *
 * @note The global variable `image` should be a valid `FILE*` that is already open.
 *       The function sets `errno` to `EINVAL` if `image` or `data` is NULL.
 */
static ssize_t read(void *data, size_t size){
    size_t bytes_read;
    
    if (image == NULL || data == NULL) {
        errno = EINVAL;
        return -1;
    }

    bytes_read = fread(data, 1, size, image);

    if(bytes_read < size){
        if (feof(image)){
            return bytes_read;
        }
        if(ferror(image)){
            return -1;
        }
    }
    return bytes_read;
}

/** 
 * @var fwimg_file_reader
 * @brief Global instance of image_reader_t that defines the standard file reader
 * for firmware images.
 */
image_reader_t fwimg_file_reader = {
    .open = open,
    .close = close,
    .read = read
};