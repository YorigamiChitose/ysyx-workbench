#include <common.h>

#if defined(MULTIPROGRAM) && !defined(TIME_SHARING)
# define MULTIPROGRAM_YIELD() yield()
#else
# define MULTIPROGRAM_YIELD()
#endif

#define NAME(key) \
  [AM_KEY_##key] = #key,

static const char *keyname[256] __attribute__((used)) = {
  [AM_KEY_NONE] = "NONE",
  AM_KEYS(NAME)
};

size_t serial_write(const void *buf, size_t offset, size_t len) {
  (void)offset;
  const char *str = buf;
  for (size_t i = 0; i < len; i++) {
    putch(str[i]);
  }
  return len;
}

size_t events_read(void *buf, size_t offset, size_t len) {
  (void)offset;
  if (len == 0) return 0;

  AM_INPUT_KEYBRD_T key = io_read(AM_INPUT_KEYBRD);
  if (key.keycode == AM_KEY_NONE || (unsigned)key.keycode >= 256) return 0;

  int n = snprintf(buf, len, "k%c %s\n", key.keydown ? 'd' : 'u', keyname[key.keycode]);
  if (n <= 0) return 0;
  return (size_t)n < len ? (size_t)n : len - 1;
}

size_t dispinfo_read(void *buf, size_t offset, size_t len) {
  if (len == 0) return 0;

  AM_GPU_CONFIG_T gpu = io_read(AM_GPU_CONFIG);
  char info[64];
  int total = snprintf(info, sizeof(info), "WIDTH : %d\nHEIGHT : %d\n", gpu.width, gpu.height);
  if (total <= 0) return 0;

  size_t size = (size_t)total;
  if (offset >= size) return 0;
  size_t n = size - offset;
  if (n > len) n = len;
  memcpy(buf, info + offset, n);
  return n;
}

size_t fb_write(const void *buf, size_t offset, size_t len) {
  AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
  assert(cfg.width > 0 && cfg.height > 0);
  assert(offset % sizeof(uint32_t) == 0);
  assert(len % sizeof(uint32_t) == 0);

  size_t total = (size_t)cfg.vmemsz;
  assert(offset <= total && len <= total - offset);

  const uint32_t *pixels = buf;
  size_t pixel = offset / sizeof(uint32_t);
  size_t remain = len / sizeof(uint32_t);

  while (remain > 0) {
    size_t x = pixel % cfg.width;
    size_t y = pixel / cfg.width;
    assert(y < (size_t)cfg.height);

    size_t w = cfg.width - x;
    if (w > remain) w = remain;
    remain -= w;
    pixel += w;

    io_write(AM_GPU_FBDRAW, (int)x, (int)y, (void *)pixels,
             (int)w, 1, remain == 0);
    pixels += w;
  }

  return len;
}

void init_device() {
  Log("Initializing devices...");
  ioe_init();
}
