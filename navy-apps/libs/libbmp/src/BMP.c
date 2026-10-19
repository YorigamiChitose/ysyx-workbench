#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

struct BitmapHeader {
  uint16_t type;
  uint32_t filesize;
  uint32_t resv_1;
  uint32_t offset;
  uint32_t ih_size;
  uint32_t width;
  uint32_t height;
  uint16_t planes;
  uint16_t bitcount; // 24 or 32
  uint32_t compression;
  uint32_t sizeimg;
  uint32_t xres, yres;
  uint32_t clrused, clrimportant;
} __attribute__((packed));

void* BMP_Load(const char *filename, int *width, int *height) {
  if (filename == NULL) return NULL;

  FILE *fp = fopen(filename, "rb");
  if (fp == NULL) return NULL;

  struct BitmapHeader hdr;
  assert(sizeof(hdr) == 54);
  if (fread(&hdr, sizeof(hdr), 1, fp) != 1) {
    fclose(fp);
    return NULL;
  }

  if (hdr.type != 0x4d42 || hdr.compression != 0 ||
      (hdr.bitcount != 24 && hdr.bitcount != 32)) {
    fclose(fp);
    return NULL;
  }

  int32_t w = (int32_t)hdr.width;
  int32_t h = (int32_t)hdr.height;
  bool top_down = h < 0;
  if (h < 0) h = -h;
  if (w <= 0 || h <= 0) {
    fclose(fp);
    return NULL;
  }
  if ((uint64_t)w * (uint64_t)h > SIZE_MAX / sizeof(uint32_t)) {
    fclose(fp);
    return NULL;
  }

  size_t depth = hdr.bitcount / 8;
  size_t line_off = depth == 4 ? (size_t)w * 4 : ((size_t)w * 3 + 3) & ~(size_t)3;
  size_t row_size = (size_t)w * depth;
  uint8_t *row = malloc(row_size);
  uint32_t *pixels = calloc((size_t)w * h, sizeof(uint32_t));
  if (row == NULL || pixels == NULL) {
    free(row);
    free(pixels);
    fclose(fp);
    return NULL;
  }

  for (int32_t i = 0; i < h; i++) {
    int32_t row_index = top_down ? i : h - 1 - i;
    if (fseek(fp, (long)(hdr.offset + (uint64_t)row_index * line_off), SEEK_SET) != 0 ||
        fread(row, depth, (size_t)w, fp) != (size_t)w) {
      free(row);
      free(pixels);
      fclose(fp);
      return NULL;
    }

    for (int32_t j = 0; j < w; j++) {
      uint8_t b = row[depth * j];
      uint8_t g = row[depth * j + 1];
      uint8_t r = row[depth * j + 2];
      pixels[(size_t)i * w + j] = (r << 16) | (g << 8) | b;
    }
  }

  free(row);
  fclose(fp);
  if (width) *width = w;
  if (height) *height = h;
  return pixels;
}
