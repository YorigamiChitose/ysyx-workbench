#include <klib.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

size_t strlen(const char *s) {
  const char *p = s;
  while (*p != '\0') p ++;
  return p - s;
}

char *strcpy(char *dst, const char *src) {
  char *ret = dst;
  while ((*dst ++ = *src ++) != '\0');
  return ret;
}

char *strncpy(char *dst, const char *src, size_t n) {
  char *ret = dst;
  size_t i;
  for (i = 0; i < n && src[i] != '\0'; i ++) dst[i] = src[i];
  for (; i < n; i ++) dst[i] = '\0';
  return ret;
}

char *strcat(char *dst, const char *src) {
  char *ret = dst;
  while (*dst != '\0') dst ++;
  while ((*dst ++ = *src ++) != '\0');
  return ret;
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 != '\0' && *s1 == *s2) { s1 ++; s2 ++; }
  return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  while (n -- > 0) {
    unsigned char c1 = (unsigned char)*s1 ++;
    unsigned char c2 = (unsigned char)*s2 ++;
    if (c1 != c2) return (int)c1 - (int)c2;
    if (c1 == '\0') return 0;
  }
  return 0;
}

void *memset(void *s, int c, size_t n) {
  unsigned char *p = s;
  while (n -- > 0) *p ++ = (unsigned char)c;
  return s;
}

void *memmove(void *dst, const void *src, size_t n) {
  unsigned char *d = dst;
  const unsigned char *s = src;
  if (d < s) {
    while (n -- > 0) *d ++ = *s ++;
  } else {
    d += n;
    s += n;
    while (n -- > 0) *--d = *--s;
  }
  return dst;
}

void *memcpy(void *out, const void *in, size_t n) {
  unsigned char *d = out;
  const unsigned char *s = in;
  while (n -- > 0) *d ++ = *s ++;
  return out;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *p1 = s1;
  const unsigned char *p2 = s2;
  while (n -- > 0) {
    if (*p1 != *p2) return (int)*p1 - (int)*p2;
    p1 ++;
    p2 ++;
  }
  return 0;
}

char *strchr(const char *s, int c) {
  do {
    if (*s == c) return (char *)s;
    if (*s == '\0') break;
    s ++;
  } while (1);
  return NULL;
}

char *strrchr(const char *s, int c) {
  const char *p = s + strlen(s);
  do {
    if (*p == c) return (char *)p;
    if (s == p) break;
    p --;
  } while (1);
  return NULL;
}

#endif
