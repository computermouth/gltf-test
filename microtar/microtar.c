/*
 * Copyright (c) 2017 rxi
 * Copyright (c) 2019 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "microtar.h"

typedef struct {
  char name[MTAR_NAMEMAX + 1];
  char mode[8];
  char owner[8];
  char group[8];
  char size[12];
  char mtime[12];
  char checksum[8];
  char type;
  char linkname[MTAR_NAMEMAX + 1];
  char _padding[255];
} mtar_raw_header_t;

static size_t mtar_round_up(size_t n, size_t incr) {
  return n + (incr - n % incr) % incr;
}

static unsigned mtar_checksum(const mtar_raw_header_t* rh) {
  unsigned i;
  const unsigned char *p = (const unsigned char*) rh;
  unsigned res = 256;
  for (i = 0; i < offsetof(mtar_raw_header_t, checksum); i++) {
    res += p[i];
  }
  for (i = offsetof(mtar_raw_header_t, type); i < sizeof(*rh); i++) {
    res += p[i];
  }
  return res;
}

static int mtar_tread(mtar_t *tar, void *data, size_t size) {
  int err = tar->read(tar, data, size);
  tar->pos += size;
  return err;
}

static int mtar_twrite(mtar_t *tar, const void *data, size_t size) {
  int err = tar->write(tar, data, size);
  tar->pos += size;
  return err;
}

static int mtar_write_null_bytes(mtar_t *tar, size_t n) {
  size_t i;
  int err;
  char nul = '\0';
  for (i = 0; i < n; i++) {
    err = mtar_twrite(tar, &nul, 1);
    if (err) {
      return err;
    }
  }
  return MTAR_ESUCCESS;
}

static int mtar_raw_to_header(mtar_header_t *h, const mtar_raw_header_t *rh) {
  unsigned chksum1, chksum2;
#ifdef HAVE_LONG_LONG
  long long size;
#else
  long size;
#endif

  if (strlen(rh->name) > MTAR_NAMEMAX || strlen(rh->linkname) > MTAR_NAMEMAX)
    return MTAR_ENAMELONG;

  /* If the checksum starts with a null byte we assume the record is NULL */
  if (*rh->checksum == '\0') {
    return MTAR_ENULLRECORD;
  }

  /* Build and compare checksum */
  chksum1 = mtar_checksum(rh);
  sscanf(rh->checksum, "%7o", &chksum2);
  if (chksum1 != chksum2) {
    return MTAR_EBADCHKSUM;
  }

  /* Load raw header into header */
  sscanf(rh->mode, "%7o", &h->mode);
  sscanf(rh->owner, "%7o", &h->owner);
  size = 0;
#ifdef HAVE_LONG_LONG
  sscanf(rh->size, "%11llo", &size);
  h->size = (size_t)size;
#else
  sscanf(rh->size, "%11lo", &size);
  h->size = (size_t)size;
#endif
  sscanf(rh->mtime, "%11o", &h->mtime);
  h->type = (unsigned)rh->type;

  if (h->size > MTAR_SIZEMAX)
    return MTAR_ETOOLARGE;

  strcpy(h->name, rh->name);
  strcpy(h->linkname, rh->linkname);

  return MTAR_ESUCCESS;
}

static int mtar_header_to_raw(mtar_raw_header_t *rh, const mtar_header_t *h) {
  unsigned chksum;

  if (h->size > MTAR_SIZEMAX) {
    return MTAR_ETOOLARGE;
  }

  /* Load header into raw header */
  memset(rh, 0, sizeof(*rh));
  sprintf(rh->mode, "%o", h->mode);
  sprintf(rh->owner, "%o", h->owner);
#ifdef HAVE_LONG_LONG
  sprintf(rh->size, "%llo", (unsigned long long)h->size);
#else
  sprintf(rh->size, "%lo", (unsigned long)h->size);
#endif
  sprintf(rh->mtime, "%o", h->mtime);
  rh->type = (char)(h->type ? h->type : MTAR_TREG);

  if (strlen(h->name) > MTAR_NAMEMAX || strlen(h->linkname) > MTAR_NAMEMAX)
    return MTAR_ENAMELONG;

  strcpy(rh->name, h->name);
  strcpy(rh->linkname, h->linkname);

  /* Calculate and write checksum */
  chksum = mtar_checksum(rh);
  sprintf(rh->checksum, "%06o", chksum);
  rh->checksum[7] = ' ';

  return MTAR_ESUCCESS;
}

const char* mtar_strerror(int err) {
  switch (err) {
    case MTAR_ESUCCESS     : return "success";
    case MTAR_EFAILURE     : return "failure";
    case MTAR_EOPENFAIL    : return "could not open";
    case MTAR_EREADFAIL    : return "could not read";
    case MTAR_EWRITEFAIL   : return "could not write";
    case MTAR_ESEEKFAIL    : return "could not seek";
    case MTAR_EBADCHKSUM   : return "bad checksum";
    case MTAR_ENULLRECORD  : return "null record";
    case MTAR_ENOTFOUND    : return "file not found";
    case MTAR_ENAMELONG    : return "name too long";
    case MTAR_ETOOLARGE    : return "file too large";
  }
  return "unknown error";
}

static int mtar_file_write(mtar_t *tar, const void *data, size_t size) {
  size_t res = fwrite(data, 1, size, (FILE *)tar->stream);
  return (res == size) ? MTAR_ESUCCESS : MTAR_EWRITEFAIL;
}

static int mtar_file_read(mtar_t *tar, void *data, size_t size) {
  size_t res = fread(data, 1, size, (FILE *)tar->stream);
  return (res == size) ? MTAR_ESUCCESS : MTAR_EREADFAIL;
}

static int mtar_file_seek(mtar_t *tar, size_t offset) {
#if defined(HAVE__FSEEKI64) && (defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__))
  int res = _fseeki64((FILE *)tar->stream, offset, SEEK_SET);
#elif defined(HAVE_FSEEKO) && (defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__))
  int res = fseeko((FILE *)tar->stream, offset, SEEK_SET);
#else
  int res = fseek((FILE *)tar->stream, offset, SEEK_SET);
#endif
  return (res == 0) ? MTAR_ESUCCESS : MTAR_ESEEKFAIL;
}

static int mtar_file_close(mtar_t *tar) {
  fclose((FILE *)tar->stream);
  return MTAR_ESUCCESS;
}

int mtar_open_fp(mtar_t *tar, void *fp) {
  /* Init tar struct and functions */
  memset(tar, 0, sizeof(*tar));
  if (!fp) return MTAR_EOPENFAIL;

  tar->write = mtar_file_write;
  tar->read = mtar_file_read;
  tar->seek = mtar_file_seek;
  tar->close = mtar_file_close;
  tar->stream = fp;

  /* Return ok */
  return MTAR_ESUCCESS;
}

int mtar_open(mtar_t *tar, const char *filename, const char *mode) {
  FILE *fp;
  int err;
  mtar_header_t h;

  /* Assure mode is always binary */
  if ( strchr(mode, 'r') ) mode = "rb";
  if ( strchr(mode, 'w') ) mode = "wb";
  if ( strchr(mode, 'a') ) mode = "ab";

  /* Open file */
  fp = fopen(filename, mode);
  if (!fp) {
    return MTAR_EOPENFAIL;
  }

  err = mtar_open_fp(tar, fp);
  if (err) {
    return err;
  }

  /* Read first header to check it is valid if mode is `r` */
  if (*mode == 'r') {
    err = mtar_read_header(tar, &h);
    if (err != MTAR_ESUCCESS) {
      mtar_close(tar);
      return err;
    }
  }

  /* Return ok */
  return MTAR_ESUCCESS;
}

#ifdef _WIN32
  int mtar_open_w(mtar_t *tar, const wchar_t *filename, const wchar_t *mode) {
    FILE *fp;
    int err;
    mtar_header_t h;

    /* Assure mode is always binary */
    if ( wcschr(mode, L'r') ) mode = L"rb";
    if ( wcschr(mode, L'w') ) mode = L"wb";
    if ( wcschr(mode, L'a') ) mode = L"ab";

    /* Open file */
    fp = _wfopen(filename, mode);
    if (!fp) {
      return MTAR_EOPENFAIL;
    }

    err = mtar_open_fp(tar, fp);
    if (err) {
      return err;
    }

    /* Read first header to check it is valid if mode is `r` */
    if (*mode == L'r') {
      err = mtar_read_header(tar, &h);
      if (err != MTAR_ESUCCESS) {
        mtar_close(tar);
        return err;
      }
    }

    /* Return ok */
    return MTAR_ESUCCESS;
  }
#endif

int mtar_close(mtar_t *tar) {
  if (tar->close)
    return tar->close(tar);
  return MTAR_ESUCCESS;
}

int mtar_seek(mtar_t *tar, size_t pos) {
  int err = tar->seek(tar, pos);
  tar->pos = pos;
  return err;
}

int mtar_rewind(mtar_t *tar) {
  tar->remaining_data = 0;
  tar->last_header = 0;
  return mtar_seek(tar, 0);
}

int mtar_next(mtar_t *tar) {
  int err;
  size_t n;
  mtar_header_t h;
  /* Load header */
  err = mtar_read_header(tar, &h);
  if (err) {
    return err;
  }
  /* Seek to next record */
  n = mtar_round_up(h.size, 512) + sizeof(mtar_raw_header_t);
  return mtar_seek(tar, tar->pos + n);
}

int mtar_find(mtar_t *tar, const char *name, mtar_header_t *h) {
  int err;
  mtar_header_t header;

  if (strlen(name) > MTAR_NAMEMAX)
    return MTAR_ENAMELONG;

  /* Start at beginning */
  err = mtar_rewind(tar);
  if (err) {
    return err;
  }
  /* Iterate all files until we hit an error or find the file */
  while ( (err = mtar_read_header(tar, &header)) == MTAR_ESUCCESS ) {
    if ( !strcmp(header.name, name) ) {
      if (h) {
        *h = header;
      }
      return MTAR_ESUCCESS;
    }
    mtar_next(tar);
  }
  /* Return error */
  if (err == MTAR_ENULLRECORD) {
    err = MTAR_ENOTFOUND;
  }
  return err;
}

int mtar_read_header(mtar_t *tar, mtar_header_t *h) {
  int err;
  mtar_raw_header_t rh;
  /* Save header position */
  tar->last_header = tar->pos;
  /* Read raw header */
  err = mtar_tread(tar, &rh, sizeof(rh));
  if (err) {
    return err;
  }
  /* Seek back to start of header */
  err = mtar_seek(tar, tar->last_header);
  if (err) {
    return err;
  }
  /* Load raw header into header struct and return */
  return mtar_raw_to_header(h, &rh);
}

int mtar_read_data(mtar_t *tar, void *ptr, size_t size) {
  int err;
  mtar_header_t h;
  /* If we have no remaining data then this is the first read, we get the size,
   * set the remaining data and seek to the beginning of the data */
  if (tar->remaining_data == 0) {
    /* Read header */
    err = mtar_read_header(tar, &h);
    if (err) {
      return err;
    }
    /* Seek past header and init remaining data */
    err = mtar_seek(tar, tar->pos + sizeof(mtar_raw_header_t));
    if (err) {
      return err;
    }
    tar->remaining_data = h.size;
  }
  /* Read data */
  err = mtar_tread(tar, ptr, size);
  if (err) {
    return err;
  }
  tar->remaining_data -= size;
  /* If there is no remaining data we've finished reading and seek back to the
   * header */
  if (tar->remaining_data == 0) {
    return mtar_seek(tar, tar->last_header);
  }
  return MTAR_ESUCCESS;
}

int mtar_write_header(mtar_t *tar, const mtar_header_t *h) {
  mtar_raw_header_t rh;
  /* Build raw header and write */
  mtar_header_to_raw(&rh, h);
  tar->remaining_data = h->size;
  return mtar_twrite(tar, &rh, sizeof(rh));
}

int mtar_write_file_header(mtar_t *tar, const char *name, size_t size) {
  mtar_header_t h;
  if (strlen(name) > MTAR_NAMEMAX)
    return MTAR_ENAMELONG;
  /* Build header */
  memset(&h, 0, sizeof(h));
  strcpy(h.name, name);
  h.size = size;
  h.type = MTAR_TREG;
  h.mode = 0664;
  /* Write header */
  return mtar_write_header(tar, &h);
}

int mtar_write_dir_header(mtar_t *tar, const char *name) {
  mtar_header_t h;
  if (strlen(name) > MTAR_NAMEMAX)
    return MTAR_ENAMELONG;
  /* Build header */
  memset(&h, 0, sizeof(h));
  strcpy(h.name, name);
  h.type = MTAR_TDIR;
  h.mode = 0775;
  /* Write header */
  return mtar_write_header(tar, &h);
}

int mtar_write_data(mtar_t *tar, const void *data, size_t size) {
  int err;
  /* Write data */
  err = mtar_twrite(tar, data, size);
  if (err) {
    return err;
  }
  tar->remaining_data -= size;
  /* Write padding if we've written all the data for this file */
  if (tar->remaining_data == 0) {
    return mtar_write_null_bytes(tar, mtar_round_up(tar->pos, 512) - tar->pos);
  }
  return MTAR_ESUCCESS;
}

int mtar_finalize(mtar_t *tar) {
  /* Write two NULL records */
  return mtar_write_null_bytes(tar, sizeof(mtar_raw_header_t) * 2);
}

static int memory_write(mtar_t *tar, const void *data, size_t size) {
  char *memory;
  size_t request_size, new_capacity;

  if (tar->stream)
    return MTAR_EWRITEFAIL;

  if (!size)
    return MTAR_ESUCCESS;

  request_size = tar->memory_pos + size;
  if (request_size > tar->memory_capacity) {
    if (request_size <= 1024)
      new_capacity = 1024;
    else
      new_capacity = request_size * 2;
    memory = (char *)realloc(tar->memory, new_capacity);
    if (!memory) {
      return MTAR_EWRITEFAIL;
    }
    tar->memory = memory;
    tar->memory_size = request_size;
    tar->memory_capacity = new_capacity;
  } else if (request_size > tar->memory_size) {
    memory = (char *)tar->memory;
    tar->memory_size = request_size;
  } else {
    memory = (char *)tar->memory;
    if (!memory) {
      return MTAR_EWRITEFAIL;
    }
  }

  memcpy(&memory[tar->memory_pos], data, size);
  tar->memory_pos += size;

  return MTAR_ESUCCESS;
}

static int memory_read(mtar_t *tar, void *data, size_t size) {
  char *memory;

  if (!size)
    return MTAR_ESUCCESS;

  memory = (char *)tar->stream;
  if (tar->memory_pos + size > tar->memory_size)
    return MTAR_EREADFAIL;

  memcpy(data, &memory[tar->memory_pos], size);
  tar->memory_pos += size;

  return MTAR_ESUCCESS;
}

static int memory_seek(mtar_t *tar, size_t offset) {
  if (!tar->stream)
    return MTAR_ESEEKFAIL;

  if (offset > tar->memory_size)
    return MTAR_ESEEKFAIL;

  tar->memory_pos = offset;
  return MTAR_ESUCCESS;
}

static int memory_close(mtar_t *tar) {
  free(tar->memory);
  tar->memory = NULL;
  return MTAR_ESUCCESS;
}

int mtar_open_memory(mtar_t *tar, void *data, size_t size) {
  if (!data || !size) {
    data = NULL;
    size = 0;
  }

  /* Init tar struct and functions */
  memset(tar, 0, sizeof(*tar));
  tar->write = memory_write;
  tar->read = memory_read;
  tar->seek = memory_seek;
  tar->close = memory_close;
  tar->stream = data;   /* for input */
  tar->memory = NULL;   /* for output */
  tar->memory_pos = 0;
  tar->memory_size = size;
  tar->memory_capacity = 0;

  /* Return ok */
  return MTAR_ESUCCESS;
}
