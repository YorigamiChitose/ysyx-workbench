#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

static int evtdev = -1;
static int fbdev = -1;
static int screen_w = 0, screen_h = 0;
static int cave_w = 0, cave_h = 0;
static uint64_t NDL_start_time = 0;

static uint64_t get_time_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

uint32_t NDL_GetTicks() {
  return (uint32_t)(get_time_ms() - NDL_start_time);
}

int NDL_PollEvent(char *buf, int len) {
  if (buf == NULL || len <= 0) return 0;
  if (evtdev < 0) {
    evtdev = open("/dev/events", O_RDONLY);
  }
  if (evtdev < 0) return 0;

  int nread = read(evtdev, buf, len);
  if (nread > 0 && nread < len) {
    buf[nread] = '\0';
  }
  return nread > 0 ? nread : 0;
}

void NDL_OpenCanvas(int *w, int *h) {
  if (w == NULL || h == NULL) return;

  if (*w <= 0 || *h <= 0) {
    *w = screen_w;
    *h = screen_h;
  }
  cave_w = *w;
  cave_h = *h;

  if (getenv("NWM_APP")) {
    int fbctl = 4;
    fbdev = 5;
    screen_w = *w;
    screen_h = *h;
    char buf[64];
    int len = sprintf(buf, "%d %d", screen_w, screen_h);
    write(fbctl, buf, len);
    while (1) {
      int nread = read(3, buf, sizeof(buf) - 1);
      if (nread <= 0) continue;
      buf[nread] = '\0';
      if (strcmp(buf, "mmap ok") == 0) break;
    }
    close(fbctl);
  }
}

void NDL_DrawRect(uint32_t *pixels, int x, int y, int w, int h) {
  if (pixels == NULL || w <= 0 || h <= 0) return;
  if (fbdev < 0) {
    fbdev = open("/dev/fb", O_RDWR);
  }
  if (fbdev < 0) return;

  x += (screen_w - cave_w) / 2;
  y += (screen_h - cave_h) / 2;

  int src_w = w;
  if (x < 0) {
    pixels += -x;
    w += x;
    x = 0;
  }
  if (y < 0) {
    pixels += -y * src_w;
    h += y;
    y = 0;
  }
  if (x + w > screen_w) w = screen_w - x;
  if (y + h > screen_h) h = screen_h - y;
  if (w <= 0 || h <= 0) return;

  for (int i = 0; i < h; i++) {
    lseek(fbdev, (x + (y + i) * screen_w) * sizeof(uint32_t), SEEK_SET);
    write(fbdev, pixels + i * src_w, w * sizeof(uint32_t));
  }
}

void NDL_OpenAudio(int freq, int channels, int samples) {
  (void)freq;
  (void)channels;
  (void)samples;
}

void NDL_CloseAudio() {
}

int NDL_PlayAudio(void *buf, int len) {
  (void)buf;
  (void)len;
  return 0;
}

int NDL_QueryAudio() {
  return 0;
}

int NDL_Init(uint32_t flags) {
  (void)flags;
  if (getenv("NWM_APP")) {
    evtdev = 3;
  }

  NDL_start_time = get_time_ms();

  int fd = open("/proc/dispinfo", O_RDONLY);
  if (fd >= 0) {
    char buf[128] = {0};
    if (read(fd, buf, sizeof(buf) - 1) > 0) {
      sscanf(buf, "WIDTH : %d\nHEIGHT : %d\n", &screen_w, &screen_h);
      cave_w = screen_w;
      cave_h = screen_h;
    }
    close(fd);
  }
  return 0;
}

void NDL_Quit() {
  if (evtdev >= 0) close(evtdev);
  if (fbdev >= 0) close(fbdev);
  evtdev = -1;
  fbdev = -1;
}
