#include <common.h>

extern uint8_t ramdisk_start;
extern uint8_t ramdisk_end;

static size_t ramdisk_size(void) {
  return (size_t)((uintptr_t)&ramdisk_end - (uintptr_t)&ramdisk_start);
}

static bool ramdisk_range_valid(size_t offset, size_t len) {
  size_t size = ramdisk_size();
  return offset <= size && len <= size - offset;
}

/* The kernel is monolithic, therefore we do not need to
 * translate the address `buf' from the user process to
 * a physical one, which is necessary for a microkernel.
 */

/* read `len' bytes starting from `offset' of ramdisk into `buf' */
size_t ramdisk_read(void *buf, size_t offset, size_t len) {
  assert(ramdisk_range_valid(offset, len));
  if (len == 0) return 0;
  memcpy(buf, &ramdisk_start + offset, len);
  return len;
}

/* write `len' bytes starting from `buf' into the `offset' of ramdisk */
size_t ramdisk_write(const void *buf, size_t offset, size_t len) {
  assert(ramdisk_range_valid(offset, len));
  if (len == 0) return 0;
  memcpy(&ramdisk_start + offset, buf, len);
  return len;
}

void init_ramdisk() {
  Log("ramdisk info: start = %p, end = %p, size = %zu bytes",
      &ramdisk_start, &ramdisk_end, ramdisk_size());
}

size_t get_ramdisk_size() {
  return ramdisk_size();
}
