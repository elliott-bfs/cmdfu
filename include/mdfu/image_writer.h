#ifndef IMAGE_WRITER_H
#define IMAGE_WRITER_H

#include <stdio.h>

/**
 * @struct image_writer_t
 * @brief Structure defining the file writer interface for firmware images.
 *
 * This structure provides function pointers to the operations necessary to
 * interact with firmware image files. It allows for abstraction of the file
 * writing process, so different file writer implementations can be used
 * without changing the code that uses them.
 */
typedef struct image_writer {
    int (* open)(const char *fpath);
    int (* close)(void);
    ssize_t (* write)(void *data, size_t size);
}image_writer_t;


extern image_writer_t fwimg_file_writer;

#endif