#include <fs.h>

typedef size_t (*ReadFn) (void *buf, size_t offset, size_t len);
typedef size_t (*WriteFn) (const void *buf, size_t offset, size_t len);

typedef struct {
  const char *name;
  size_t size;
  size_t disk_offset;
  ReadFn read;
  WriteFn write;
} Finfo;

typedef struct {
  size_t open_offset;
} SEEKinfo;

enum {FD_STDIN, FD_STDOUT, FD_STDERR, FD_FB, FD_EVENTS, FD_DISPINFO, FD_END};

size_t ramdisk_read(void *buf, size_t offset, size_t len);
size_t ramdisk_write(const void *buf, size_t offset, size_t len);
size_t serial_write(const void *buf, size_t offset, size_t len);
size_t events_read(void *buf, size_t offset, size_t len);
size_t dispinfo_read(void *buf, size_t offset, size_t len);
size_t fb_write(const void *buf, size_t offset, size_t len);

size_t invalid_read(void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

size_t invalid_write(const void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

/* This is the information about all files in disk. */
static Finfo file_table[] __attribute__((used)) = {
  [FD_STDIN   ] = {"stdin",           0, 0, invalid_read,  invalid_write },
  [FD_STDOUT  ] = {"stdout",          0, 0, invalid_read,  serial_write  },
  [FD_STDERR  ] = {"stderr",          0, 0, invalid_read,  serial_write  },
  [FD_FB      ] = {"/dev/fb",         0, 0, invalid_read,  fb_write      },
  [FD_EVENTS  ] = {"/dev/events",     0, 0, events_read,   invalid_write },
  [FD_DISPINFO] = {"/proc/dispinfo",  0, 0, dispinfo_read, invalid_write },
#include "files.h"
};

static SEEKinfo file_offset[LENGTH(file_table)] = {0};

static bool fd_valid(int fd) {
  return fd >= 0 && (size_t)fd < LENGTH(file_table);
}

void init_fs() {
  file_table[FD_FB].size = io_read(AM_GPU_CONFIG).vmemsz;
}

int fs_open(const char *pathname, int flags, int mode) {
  (void)flags;
  (void)mode;
  if (pathname == NULL) return -1;

  for (size_t i = 0; i < LENGTH(file_table); i++) {
    if (strcmp(pathname, file_table[i].name) == 0) {
      file_offset[i].open_offset = 0;
      return (int)i;
    }
  }

  panic("file not found: %s", pathname);
  return -1;
}

size_t fs_read(int fd, void *buf, size_t len) {
  assert(fd_valid(fd));
  if (len == 0) return 0;
  assert(buf != NULL);

  Finfo *file = &file_table[fd];
  size_t offset = file_offset[fd].open_offset;
  size_t read_len;
  if (file->read != NULL) {
    read_len = file->read(buf, file->disk_offset + offset, len);
  } else {
    if (offset >= file->size) return 0;
    if (len > file->size - offset) {
      len = file->size - offset;
    }
    read_len = ramdisk_read(buf, file->disk_offset + offset, len);
  }

  file_offset[fd].open_offset += read_len;
  return read_len;
}

size_t fs_write(int fd, const void *buf, size_t len) {
  assert(fd_valid(fd));
  if (len == 0) return 0;
  assert(buf != NULL);

  Finfo *file = &file_table[fd];
  size_t offset = file_offset[fd].open_offset;
  if (file->write != NULL) {
    return file->write(buf, file->disk_offset + offset, len);
  }

  if (offset >= file->size) return 0;
  if (len > file->size - offset) {
    len = file->size - offset;
  }
  size_t write_len = ramdisk_write(buf, file->disk_offset + offset, len);
  file_offset[fd].open_offset += write_len;
  return write_len;
}

size_t fs_lseek(int fd, intptr_t offset, int whence) {
  assert(fd_valid(fd));

  Finfo *file = &file_table[fd];
  intptr_t base;
  switch (whence) {
    case SEEK_SET: base = 0; break;
    case SEEK_CUR: base = (intptr_t)file_offset[fd].open_offset; break;
    case SEEK_END: base = (intptr_t)file->size; break;
    default: panic("invalid whence: %d", whence); return 0;
  }

  intptr_t next = base + offset;
  assert(next >= 0);
  file_offset[fd].open_offset = (size_t)next;
  return (size_t)next;
}

int fs_close(int fd) {
  assert(fd_valid(fd));
  file_offset[fd].open_offset = 0;
  return 0;
}
