#include <common.h>
#include <fs.h>
#include <sys/time.h>
#include "syscall.h"

void execve_load(const char *filename, char *const argv[], char *const envp[]);

static intptr_t sys_brk(uintptr_t increment) {
  (void)increment;
  return 0;
}

static int sys_gettimeofday(struct timeval *tv, struct timezone *tz) {
  AM_TIMER_UPTIME_T t = io_read(AM_TIMER_UPTIME);
  if (tv != NULL) {
    tv->tv_sec = t.us / 1000000;
    tv->tv_usec = t.us % 1000000;
  }
  if (tz != NULL) {
    tz->tz_minuteswest = -480;
    tz->tz_dsttime = DST_NONE;
  }
  return 0;
}

void do_syscall(Context *c) {
  uintptr_t a[4] = {c->GPR1, c->GPR2, c->GPR3, c->GPR4};

  switch (a[0]) {
    case SYS_exit:
      halt((int)a[1]);
      break;
    case SYS_yield:
      c->GPRx = 0;
      break;
    case SYS_open:
      c->GPRx = fs_open((const char *)a[1], (int)a[2], (int)a[3]);
      break;
    case SYS_read:
      c->GPRx = fs_read((int)a[1], (void *)a[2], a[3]);
      break;
    case SYS_write:
      c->GPRx = fs_write((int)a[1], (const void *)a[2], a[3]);
      break;
    case SYS_close:
      c->GPRx = fs_close((int)a[1]);
      break;
    case SYS_lseek:
      c->GPRx = fs_lseek((int)a[1], (intptr_t)a[2], (int)a[3]);
      break;
    case SYS_brk:
      c->GPRx = sys_brk(a[1]);
      break;
    case SYS_gettimeofday:
      c->GPRx = sys_gettimeofday((struct timeval *)a[1],
                                (struct timezone *)a[2]);
      break;
    case SYS_execve:
      execve_load((const char *)a[1], (char *const *)a[2], (char *const *)a[3]);
      break;
    default:
      panic("Unhandled syscall ID = %d", (int)a[0]);
      break;
  }
}
