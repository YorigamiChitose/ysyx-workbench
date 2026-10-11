#include <klib.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

/* --------------- minimal printf engine --------------- */

#define F_MINUS  (1 << 0)   /* '-' left aligned */
#define F_ZERO   (1 << 1)   /* '0' zero padded */
#define F_PLUS   (1 << 2)   /* '+' always show sign */
#define F_SPACE  (1 << 3)   /* ' ' leading space for positive */
#define F_ALT    (1 << 4)   /* '#' alternate form */

enum { L_NONE, L_HH, L_H, L_L, L_LL, L_Z };

typedef struct {
  char  *buf;   /* NULL means the output goes through putch() */
  size_t pos;   /* characters produced so far, excluding the terminating '\0' */
  size_t cap;   /* buffer capacity including the terminating '\0' */
} sink_t;

static void emitc(sink_t *s, char ch) {
  if (s->buf != NULL) {
    if (s->pos + 1 < s->cap) s->buf[s->pos] = ch;
  } else {
    putch(ch);
  }
  s->pos ++;
}

static void emitpad(sink_t *s, int n, char pad) {
  while (n -- > 0) emitc(s, pad);
}

static void emit_number(sink_t *s, uint64_t value, bool neg, int base, bool upper,
    int flags, int width, int prec) {
  const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
  bool is_zero = (value == 0);

  char rev[32];
  int nd = 0;
  if (is_zero) rev[nd ++] = '0';
  while (value != 0) {
    rev[nd ++] = digits[value % base];
    value /= base;
  }
  if (prec == 0 && is_zero) nd = 0;

  char sign = '\0';
  if (neg) sign = '-';
  else if (flags & F_PLUS) sign = '+';
  else if (flags & F_SPACE) sign = ' ';

  int zeros = (prec > nd) ? prec - nd : 0;

  const char *prefix = "";
  int plen = 0;
  if (flags & F_ALT) {
    if (base == 16) {
      prefix = upper ? "0X" : "0x";
      plen = 2;
    } else if (base == 8 && (nd == 0 || rev[nd - 1] != '0')) {
      zeros = (zeros < 1) ? 1 : zeros;
    }
  }
  if (sign != '\0') plen ++;

  int body = nd + zeros;
  int pad = (width > plen + body) ? width - plen - body : 0;

  if ((flags & F_ZERO) && !(flags & F_MINUS) && prec < 0) {
    if (sign != '\0') emitc(s, sign);
    while (*prefix) emitc(s, *prefix ++);
    emitpad(s, pad, '0');
  } else {
    if (!(flags & F_MINUS)) emitpad(s, pad, ' ');
    if (sign != '\0') emitc(s, sign);
    while (*prefix) emitc(s, *prefix ++);
  }

  emitpad(s, zeros, '0');
  for (int i = nd - 1; i >= 0; i --) emitc(s, rev[i]);

  if (flags & F_MINUS) emitpad(s, pad, ' ');
}

static void emit_string(sink_t *s, const char *str, int flags, int width, int prec) {
  if (str == NULL) str = "(null)";
  int len = (int)strlen(str);
  if (prec >= 0 && prec < len) len = prec;

  int pad = (width > len) ? width - len : 0;
  if (!(flags & F_MINUS)) emitpad(s, pad, (flags & F_ZERO) ? '0' : ' ');
  for (int i = 0; i < len; i ++) emitc(s, str[i]);
  if (flags & F_MINUS) emitpad(s, pad, ' ');
}

static int vformat(sink_t *s, const char *fmt, va_list ap) {
  while (*fmt != '\0') {
    if (*fmt != '%') {
      emitc(s, *fmt ++);
      continue;
    }
    fmt ++;  /* skip '%' */

    int flags = 0;
    for (;;) {
      switch (*fmt) {
        case '-': flags |= F_MINUS; break;
        case '0': flags |= F_ZERO;  break;
        case '+': flags |= F_PLUS;  break;
        case ' ': flags |= F_SPACE; break;
        case '#': flags |= F_ALT;   break;
        default: goto flags_done;
      }
      fmt ++;
    }
flags_done: ;

    int width = 0;
    while (*fmt >= '0' && *fmt <= '9') {
      width = width * 10 + (*fmt - '0');
      fmt ++;
    }

    int prec = -1;
    if (*fmt == '.') {
      fmt ++;
      prec = 0;
      while (*fmt >= '0' && *fmt <= '9') {
        prec = prec * 10 + (*fmt - '0');
        fmt ++;
      }
    }

    int length = L_NONE;
    if (*fmt == 'h') {
      length = L_H;
      fmt ++;
      if (*fmt == 'h') { length = L_HH; fmt ++; }
    } else if (*fmt == 'l') {
      length = L_L;
      fmt ++;
      if (*fmt == 'l') { length = L_LL; fmt ++; }
    } else if (*fmt == 'z') {
      length = L_Z;
      fmt ++;
    }

    char conv = *fmt;
    if (conv == '\0') break;

    switch (conv) {
      case 'd':
      case 'i': {
        int64_t v;
        if (length == L_LL)       v = (int64_t)va_arg(ap, long long);
        else if (length == L_L)   v = (int64_t)va_arg(ap, long);
        else if (length == L_Z)   v = (int64_t)(intptr_t)va_arg(ap, void *);
        else if (length == L_H)   v = (int64_t)(short)va_arg(ap, int);
        else if (length == L_HH)  v = (int64_t)(signed char)va_arg(ap, int);
        else                      v = (int64_t)va_arg(ap, int);
        bool neg = (v < 0);
        uint64_t uv = (uint64_t)v;
        if (neg) uv = 0ULL - uv;
        emit_number(s, uv, neg, 10, false, flags, width, prec);
        break;
      }
      case 'u':
      case 'x':
      case 'X':
      case 'o': {
        uint64_t uv;
        if (length == L_LL)       uv = (uint64_t)va_arg(ap, unsigned long long);
        else if (length == L_L)   uv = (uint64_t)va_arg(ap, unsigned long);
        else if (length == L_Z)   uv = (uint64_t)(uintptr_t)va_arg(ap, void *);
        else if (length == L_H)   uv = (uint64_t)(unsigned short)va_arg(ap, unsigned int);
        else if (length == L_HH)  uv = (uint64_t)(unsigned char)va_arg(ap, unsigned int);
        else                      uv = (uint64_t)va_arg(ap, unsigned int);
        int base = (conv == 'o') ? 8 : (conv == 'x' || conv == 'X') ? 16 : 10;
        emit_number(s, uv, false, base, conv == 'X', flags, width, prec);
        break;
      }
      case 'c': {
        char ch = (char)va_arg(ap, int);
        emit_string(s, (char []){ch, '\0'}, flags, width, -1);
        break;
      }
      case 's':
        emit_string(s, va_arg(ap, const char *), flags, width, prec);
        break;
      case 'p': {
        uintptr_t p = (uintptr_t)va_arg(ap, void *);
        emit_number(s, (uint64_t)p, false, 16, false,
            flags | F_ALT, width, (int)sizeof(void *) * 2);
        break;
      }
      case '%':
        emitc(s, '%');
        break;
      default:
        emitc(s, '%');
        emitc(s, conv);
        break;
    }
    fmt ++;
  }
  return (int)s->pos;
}

int vprintf(const char *fmt, va_list ap) {
  sink_t s = { .buf = NULL, .pos = 0, .cap = 0 };
  return vformat(&s, fmt, ap);
}

int printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vprintf(fmt, ap);
  va_end(ap);
  return ret;
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  sink_t s = { .buf = out, .pos = 0, .cap = (size_t)-1 };
  int ret = vformat(&s, fmt, ap);
  out[ret] = '\0';
  return ret;
}

int sprintf(char *out, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vsprintf(out, fmt, ap);
  va_end(ap);
  return ret;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  sink_t s = { .buf = out, .pos = 0, .cap = n };
  int ret = vformat(&s, fmt, ap);
  if (n > 0) out[(size_t)ret < n - 1 ? ret : (int)(n - 1)] = '\0';
  return ret;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vsnprintf(out, n, fmt, ap);
  va_end(ap);
  return ret;
}

int __am_vsscanf_internal(const char *str, const char **end_pstr, const char *fmt, va_list ap) {
  const char *pstr = str;
  const char *pfmt = fmt;
  int item = -1;
  while (*pfmt) {
    char ch = *pfmt ++;
    if (isspace(ch)) {
      for (ch = *pfmt; isspace(ch); ch = *(++ pfmt));
      for (ch = *pstr; isspace(ch); ch = *(++ pstr));
      item ++;
      continue;
    }
    switch (ch) {
      case '%': break;
      default:
        if (*pstr == ch) { // match
          pstr ++;
          item ++;
          continue;
        }
        goto end; // fail
    }

    char *p;
    ch = *pfmt ++;
    switch (ch) {
      // conversion specifier
      case 'd':
        *(va_arg(ap, int *)) = strtol(pstr, &p, 10);
        if (p == pstr) goto end; // fail
        pstr = p;
        item ++;
        break;

      case 'c':
        *(va_arg(ap, char *)) = *pstr ++;
        item ++;
        break;

      default:
        printf("Unsupported conversion specifier '%c'\n", ch);
        assert(0);
    }
  }

end:
  if (end_pstr) {
    *end_pstr = pstr;
  }
  return item;
}

int vsscanf(const char *str, const char *fmt, va_list ap) {
  return __am_vsscanf_internal(str, NULL, fmt, ap);
}

int sscanf(const char *str, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vsscanf(str, fmt, ap);
  va_end(ap);
  return r;
}

int __isoc99_sscanf(const char *str, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vsscanf(str, fmt, ap);
  va_end(ap);
  return r;
}

#endif
