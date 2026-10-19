#define SDL_malloc  malloc
#define SDL_free    free
#define SDL_realloc realloc

#define SDL_STBIMAGE_IMPLEMENTATION
#include "SDL_stbimage.h"

SDL_Surface* IMG_Load_RW(SDL_RWops *src, int freesrc) {
  assert(src != NULL);
  assert(freesrc == 0);
  if (src->type != RW_TYPE_MEM || src->mem.base == NULL || src->mem.size <= 0) {
    return NULL;
  }
  return STBIMG_LoadFromMemory(src->mem.base, (size_t)src->mem.size);
}

SDL_Surface* IMG_Load(const char *filename) {
  if (filename == NULL) return NULL;

  FILE *fp = fopen(filename, "rb");
  if (fp == NULL) return NULL;

  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return NULL;
  }
  long file_size = ftell(fp);
  if (file_size < 0 || fseek(fp, 0, SEEK_SET) != 0) {
    fclose(fp);
    return NULL;
  }

  size_t size = (size_t)file_size;
  char *img_mem = SDL_malloc(size == 0 ? 1 : size);
  if (img_mem == NULL) {
    fclose(fp);
    return NULL;
  }

  if (size != 0 && fread(img_mem, 1, size, fp) != size) {
    SDL_free(img_mem);
    fclose(fp);
    return NULL;
  }
  fclose(fp);

  SDL_Surface *img = STBIMG_LoadFromMemory(img_mem, size);
  SDL_free(img_mem);
  return img;
}

int IMG_isPNG(SDL_RWops *src) {
  (void)src;
  return 0;
}

SDL_Surface* IMG_LoadJPG_RW(SDL_RWops *src) {
  return IMG_Load_RW(src, 0);
}

char *IMG_GetError() {
  return "Navy does not support IMG_GetError()";
}
