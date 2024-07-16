#ifndef IMAGE_READER_H
#define IMAGE_READER_H

#include <stdio.h>

/**
 * @struct image_reader_t
 * @brief Structure defining the file reader interface for firmware images.
 *
 * This structure provides function pointers to the operations necessary to
 * interact with firmware image files. It allows for abstraction of the file
 * reading process, so different file reader implementations can be used
 * without changing the code that uses them.
 */
typedef struct image_reader {
    int (* open)(const char *fpath);
    int (* close)(void);
    ssize_t (* read)(void *data, size_t size);
}image_reader_t;


extern image_reader_t fwimg_file_reader;

#endif