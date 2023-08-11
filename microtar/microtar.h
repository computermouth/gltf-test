/**
 * Copyright (c) 2017 rxi
 * Copyright (c) 2019 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `microtar.c` for details.
 */

#ifndef MICROTAR_H
#define MICROTAR_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdlib.h>

#define MTAR_VERSION "0.1.3"
#define MTAR_NAMEMAX 99

#if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__)
  #define MTAR_SIZEMAX 0x1FFFFFFFFULL
#else
  #define MTAR_SIZEMAX  0x7FFFFFFFUL
#endif

enum {
  MTAR_ESUCCESS     =  0,
  MTAR_EFAILURE     = -1,
  MTAR_EOPENFAIL    = -2,
  MTAR_EREADFAIL    = -3,
  MTAR_EWRITEFAIL   = -4,
  MTAR_ESEEKFAIL    = -5,
  MTAR_EBADCHKSUM   = -6,
  MTAR_ENULLRECORD  = -7,
  MTAR_ENOTFOUND    = -8,
  MTAR_ENAMELONG    = -9,
  MTAR_ETOOLARGE    = -10
};

enum {
  MTAR_TREG   = '0',
  MTAR_TLNK   = '1',
  MTAR_TSYM   = '2',
  MTAR_TCHR   = '3',
  MTAR_TBLK   = '4',
  MTAR_TDIR   = '5',
  MTAR_TFIFO  = '6'
};

typedef struct {
  unsigned mode;
  unsigned owner;
  size_t size;
  unsigned mtime;
  unsigned type;
  char name[MTAR_NAMEMAX + 1];
  char linkname[MTAR_NAMEMAX + 1];
} mtar_header_t;

typedef struct mtar_t mtar_t;

typedef int (*mtar_read_t)(mtar_t *tar, void *data, size_t size);
typedef int (*mtar_write_t)(mtar_t *tar, const void *data, size_t size);
typedef int (*mtar_seek_t)(mtar_t *tar, size_t pos);
typedef int (*mtar_close_t)(mtar_t *tar);

struct mtar_t {
  mtar_read_t read;
  mtar_write_t write;
  mtar_seek_t seek;
  mtar_close_t close;
  void *stream;
  size_t pos;
  size_t remaining_data;
  size_t last_header;
  void *memory;         /* malloc'ed */
  size_t memory_pos;
  size_t memory_size;
  size_t memory_capacity;
};

const char* mtar_strerror(int err);

int mtar_open(mtar_t *tar, const char *filename, const char *mode);
#ifdef _WIN32
  int mtar_open_w(mtar_t *tar, const wchar_t *filename, const wchar_t *mode);
#endif
int mtar_open_fp(mtar_t *tar, void *fp);
int mtar_open_memory(mtar_t *tar, void *data, size_t size);
int mtar_close(mtar_t *tar);

int mtar_seek(mtar_t *tar, size_t pos);
int mtar_rewind(mtar_t *tar);
int mtar_next(mtar_t *tar);
int mtar_find(mtar_t *tar, const char *name, mtar_header_t *h);
int mtar_read_header(mtar_t *tar, mtar_header_t *h);
int mtar_read_data(mtar_t *tar, void *ptr, size_t size);

int mtar_write_header(mtar_t *tar, const mtar_header_t *h);
int mtar_write_file_header(mtar_t *tar, const char *name, size_t size);
int mtar_write_dir_header(mtar_t *tar, const char *name);
int mtar_write_data(mtar_t *tar, const void *data, size_t size);
int mtar_finalize(mtar_t *tar);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
    #define mtar_open_t mtar_open_w
#else
    #define mtar_open_t mtar_open
#endif

#endif
