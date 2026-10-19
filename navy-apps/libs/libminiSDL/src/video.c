#include <NDL.h>
#include <sdl-video.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

void SDL_BlitSurface(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect) {
  assert(src && dst && src->format && dst->format);
  assert(src->format->BitsPerPixel == dst->format->BitsPerPixel);

  int bpp = src->format->BitsPerPixel;
  assert(bpp == 8 || bpp == 32);

  int sx = srcrect ? srcrect->x : 0;
  int sy = srcrect ? srcrect->y : 0;
  int w = srcrect ? srcrect->w : src->w;
  int h = srcrect ? srcrect->h : src->h;
  int dx = dstrect ? dstrect->x : 0;
  int dy = dstrect ? dstrect->y : 0;

  if (w <= 0 || h <= 0) return;

  if (sx < 0) { w += sx; dx -= sx; sx = 0; }
  if (sy < 0) { h += sy; dy -= sy; sy = 0; }
  if (dx < 0) { sx -= dx; w += dx; dx = 0; }
  if (dy < 0) { sy -= dy; h += dy; dy = 0; }

  if (sx + w > src->w) w = src->w - sx;
  if (sy + h > src->h) h = src->h - sy;
  if (dx + w > dst->w) w = dst->w - dx;
  if (dy + h > dst->h) h = dst->h - dy;
  if (w <= 0 || h <= 0) return;

  if (bpp == 32) {
    uint32_t *sp = (uint32_t *)src->pixels;
    uint32_t *dp = (uint32_t *)dst->pixels;
    for (int i = 0; i < h; i++) {
      memmove(&dp[(dy + i) * dst->w + dx],
              &sp[(sy + i) * src->w + sx],
              (size_t)w * sizeof(uint32_t));
    }
  } else {
    for (int i = 0; i < h; i++) {
      memmove(&dst->pixels[(dy + i) * dst->w + dx],
              &src->pixels[(sy + i) * src->w + sx],
              (size_t)w);
    }
  }
}

void SDL_FillRect(SDL_Surface *dst, SDL_Rect *dstrect, uint32_t color) {
  assert(dst && dst->format);

  int x = dstrect ? dstrect->x : 0;
  int y = dstrect ? dstrect->y : 0;
  int w = dstrect ? dstrect->w : dst->w;
  int h = dstrect ? dstrect->h : dst->h;

  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > dst->w) w = dst->w - x;
  if (y + h > dst->h) h = dst->h - y;
  if (w <= 0 || h <= 0) return;

  int bpp = dst->format->BitsPerPixel;
  if (bpp == 32) {
    uint32_t *pixels = (uint32_t *)dst->pixels;
    for (int i = 0; i < h; i++) {
      uint32_t *row = &pixels[(y + i) * dst->w + x];
      for (int j = 0; j < w; j++) {
        row[j] = color;
      }
    }
  } else if (bpp == 8) {
    for (int i = 0; i < h; i++) {
      memset(&dst->pixels[(y + i) * dst->w + x], color, w);
    }
  }
}

void SDL_UpdateRect(SDL_Surface *s, int x, int y, int w, int h) {
  assert(s && s->format && s->pixels);

  if (w <= 0 || h <= 0) {
    x = 0;
    y = 0;
    w = s->w;
    h = s->h;
  }

  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > s->w) w = s->w - x;
  if (y + h > s->h) h = s->h - y;
  if (w <= 0 || h <= 0) return;

  int bpp = s->format->BitsPerPixel;
  if (bpp == 32) {
    uint32_t *pixels = (uint32_t *)s->pixels;
    if (x == 0 && w == s->w) {
      NDL_DrawRect(pixels + y * s->w, 0, y, w, h);
      return;
    }

    uint32_t *buf = malloc((size_t)w * h * sizeof(uint32_t));
    if (buf == NULL) return;
    for (int i = 0; i < h; i++) {
      memcpy(&buf[i * w], &pixels[(y + i) * s->w + x],
             (size_t)w * sizeof(uint32_t));
    }
    NDL_DrawRect(buf, x, y, w, h);
    free(buf);
  } else if (bpp == 8) {
    assert(s->format->palette && s->format->palette->colors);

    uint32_t *buf = malloc((size_t)w * h * sizeof(uint32_t));
    if (buf == NULL) return;
    for (int i = 0; i < h; i++) {
      for (int j = 0; j < w; j++) {
        uint8_t index = s->pixels[(y + i) * s->w + x + j];
        buf[i * w + j] = s->format->palette->colors[index].val;
      }
    }
    NDL_DrawRect(buf, x, y, w, h);
    free(buf);
  }
}

// APIs below are already implemented.

static inline int maskToShift(uint32_t mask) {
  switch (mask) {
    case 0x000000ff: return 0;
    case 0x0000ff00: return 8;
    case 0x00ff0000: return 16;
    case 0xff000000: return 24;
    case 0x00000000: return 24; // hack
    default: assert(0); return 0;
  }
}

SDL_Surface* SDL_CreateRGBSurface(uint32_t flags, int width, int height, int depth,
    uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask) {
  assert(depth == 8 || depth == 32);
  SDL_Surface *s = calloc(1, sizeof(SDL_Surface));
  assert(s);
  s->flags = flags;
  s->format = calloc(1, sizeof(SDL_PixelFormat));
  assert(s->format);
  if (depth == 8) {
    s->format->palette = calloc(1, sizeof(SDL_Palette));
    assert(s->format->palette);
    s->format->palette->colors = calloc(256, sizeof(SDL_Color));
    assert(s->format->palette->colors);
    s->format->palette->ncolors = 256;
  } else {
    s->format->palette = NULL;
    s->format->Rmask = Rmask; s->format->Rshift = maskToShift(Rmask); s->format->Rloss = 0;
    s->format->Gmask = Gmask; s->format->Gshift = maskToShift(Gmask); s->format->Gloss = 0;
    s->format->Bmask = Bmask; s->format->Bshift = maskToShift(Bmask); s->format->Bloss = 0;
    s->format->Amask = Amask; s->format->Ashift = maskToShift(Amask); s->format->Aloss = 0;
  }

  s->format->BitsPerPixel = depth;
  s->format->BytesPerPixel = depth / 8;

  s->w = width;
  s->h = height;
  s->pitch = width * depth / 8;
  assert(s->pitch == width * s->format->BytesPerPixel);

  if (!(flags & SDL_PREALLOC)) {
    s->pixels = malloc((size_t)s->pitch * height);
    assert(s->pixels);
  }

  return s;
}

SDL_Surface* SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height, int depth,
    int pitch, uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask) {
  SDL_Surface *s = SDL_CreateRGBSurface(SDL_PREALLOC, width, height, depth,
      Rmask, Gmask, Bmask, Amask);
  assert(pitch == s->pitch);
  s->pixels = pixels;
  return s;
}

void SDL_FreeSurface(SDL_Surface *s) {
  if (s != NULL) {
    if (s->format != NULL) {
      if (s->format->palette != NULL) {
        if (s->format->palette->colors != NULL) free(s->format->palette->colors);
        free(s->format->palette);
      }
      free(s->format);
    }
    if (s->pixels != NULL && !(s->flags & SDL_PREALLOC)) free(s->pixels);
    free(s);
  }
}

SDL_Surface* SDL_SetVideoMode(int width, int height, int bpp, uint32_t flags) {
  if (flags & SDL_HWSURFACE) NDL_OpenCanvas(&width, &height);
  return SDL_CreateRGBSurface(flags, width, height, bpp,
      DEFAULT_RMASK, DEFAULT_GMASK, DEFAULT_BMASK, DEFAULT_AMASK);
}

void SDL_SoftStretch(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect) {
  assert(src && dst && src->format && dst->format);
  assert(dst->format->BitsPerPixel == src->format->BitsPerPixel);
  assert(dstrect);

  int bpp = src->format->BitsPerPixel;
  assert(bpp == 8 || bpp == 32);

  int sx = srcrect ? srcrect->x : 0;
  int sy = srcrect ? srcrect->y : 0;
  int sw = srcrect ? srcrect->w : src->w;
  int sh = srcrect ? srcrect->h : src->h;
  int dw = dstrect->w;
  int dh = dstrect->h;
  if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;

  if (bpp == 8) {
    for (int i = 0; i < dh; i++) {
      int src_y = sy + i * sh / dh;
      for (int j = 0; j < dw; j++) {
        int src_x = sx + j * sw / dw;
        dst->pixels[(dstrect->y + i) * dst->w + dstrect->x + j] =
          src->pixels[src_y * src->w + src_x];
      }
    }
  } else {
    uint32_t *sp = (uint32_t *)src->pixels;
    uint32_t *dp = (uint32_t *)dst->pixels;
    for (int i = 0; i < dh; i++) {
      int src_y = sy + i * sh / dh;
      for (int j = 0; j < dw; j++) {
        int src_x = sx + j * sw / dw;
        dp[(dstrect->y + i) * dst->w + dstrect->x + j] =
          sp[src_y * src->w + src_x];
      }
    }
  }
}

void SDL_SetPalette(SDL_Surface *s, int flags, SDL_Color *colors, int firstcolor, int ncolors) {
  assert(s && s->format && s->format->palette && colors);
  assert(firstcolor >= 0 && ncolors >= 0);
  assert(firstcolor + ncolors <= 256);
  (void)flags;

  for (int i = 0; i < ncolors; i++) {
    s->format->palette->colors[firstcolor + i] = colors[i];
  }
  if (firstcolor + ncolors > s->format->palette->ncolors) {
    s->format->palette->ncolors = firstcolor + ncolors;
  }

  if (s->flags & SDL_HWSURFACE) {
    SDL_UpdateRect(s, 0, 0, 0, 0);
  }
}

static void ConvertPixelsARGB_ABGR(void *dst, void *src, int len) {
  int i;
  uint8_t (*pdst)[4] = dst;
  uint8_t (*psrc)[4] = src;
  union {
    uint8_t val8[4];
    uint32_t val32;
  } tmp;

  int first = len & ~0xf;
  for (i = 0; i < first; i += 16) {
#define macro(i) \
    tmp.val32 = *((uint32_t *)psrc[i]); \
    *((uint32_t *)pdst[i]) = tmp.val32; \
    pdst[i][0] = tmp.val8[2]; \
    pdst[i][2] = tmp.val8[0];

    macro(i + 0); macro(i + 1); macro(i + 2); macro(i + 3);
    macro(i + 4); macro(i + 5); macro(i + 6); macro(i + 7);
    macro(i + 8); macro(i + 9); macro(i +10); macro(i +11);
    macro(i +12); macro(i +13); macro(i +14); macro(i +15);
  }

  for (; i < len; i++) {
    macro(i);
  }
#undef macro
}

SDL_Surface *SDL_ConvertSurface(SDL_Surface *src, SDL_PixelFormat *fmt, uint32_t flags) {
  assert(src && src->format && fmt);
  assert(src->format->BitsPerPixel == 32);
  assert(src->w * src->format->BytesPerPixel == src->pitch);
  assert(src->format->BitsPerPixel == fmt->BitsPerPixel);

  SDL_Surface* ret = SDL_CreateRGBSurface(flags, src->w, src->h, fmt->BitsPerPixel,
    fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);

  assert(fmt->Gmask == src->format->Gmask);
  assert(fmt->Amask == 0 || src->format->Amask == 0 || (fmt->Amask == src->format->Amask));
  ConvertPixelsARGB_ABGR(ret->pixels, src->pixels, src->w * src->h);

  return ret;
}

uint32_t SDL_MapRGBA(SDL_PixelFormat *fmt, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  assert(fmt->BytesPerPixel == 4);
  uint32_t p = (r << fmt->Rshift) | (g << fmt->Gshift) | (b << fmt->Bshift);
  if (fmt->Amask) p |= (a << fmt->Ashift);
  return p;
}

int SDL_LockSurface(SDL_Surface *s) {
  (void)s;
  return 0;
}

void SDL_UnlockSurface(SDL_Surface *s) {
  (void)s;
}
