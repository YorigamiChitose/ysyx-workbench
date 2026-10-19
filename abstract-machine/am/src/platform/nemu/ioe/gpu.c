#include <am.h>
#include <nemu.h>
#include <klib.h>

#define SYNC_ADDR (VGACTL_ADDR + 4)

static uint32_t gpu_width = 0;
static uint32_t gpu_height = 0;
static uint32_t *gpu_fb = (uint32_t *)(uintptr_t)FB_ADDR;

void __am_gpu_init() {
  uint32_t info = inl(VGACTL_ADDR);
  gpu_width = info >> 16;
  gpu_height = info & 0xffff;
  memset(gpu_fb, 0, gpu_width * gpu_height * sizeof(uint32_t));
  outl(SYNC_ADDR, 1);
}

void __am_gpu_config(AM_GPU_CONFIG_T *cfg) {
  *cfg = (AM_GPU_CONFIG_T) {
    .present = true, .has_accel = false,
    .width = gpu_width, .height = gpu_height,
    .vmemsz = gpu_width * gpu_height * sizeof(uint32_t)
  };
}

void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl) {
  if (ctl->pixels != NULL && ctl->w > 0 && ctl->h > 0) {
    const uint32_t *pixels = ctl->pixels;
    for (int i = 0; i < ctl->h; i++) {
      memcpy(&gpu_fb[gpu_width * (ctl->y + i) + ctl->x],
             &pixels[ctl->w * i],
             ctl->w * sizeof(uint32_t));
    }
  }

  if (ctl->sync) {
    outl(SYNC_ADDR, 1);
  }
}

void __am_gpu_status(AM_GPU_STATUS_T *status) {
  status->ready = true;
}
