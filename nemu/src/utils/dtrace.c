#include <utils.h>
#include <common.h>

#ifdef CONFIG_DTRACE

struct DtraceNode dringbuf[CONFIG_DTRACE_BSIZE];
struct DtraceNode *dringbufNow;

void init_dringbuf(void) {
  dringbufNow = &dringbuf[CONFIG_DTRACE_BSIZE - 1];
  for (int i = 0; i < CONFIG_DTRACE_BSIZE; i++) {
    dringbuf[i].next = &dringbuf[(i + 1) % CONFIG_DTRACE_BSIZE];
    dringbuf[i].name = NULL;
    dringbuf[i].addr = 0;
    dringbuf[i].pc = 0;
    dringbuf[i].data = 0;
    dringbuf[i].len = 0;
    dringbuf[i].is_write = false;
  }
}

void display_dringbuf(void) {
  char printBuf[CONFIG_TRACE_PBSIZE];
  struct DtraceNode *dringbufTemp = dringbufNow->next;
  for (int i = 0; i < CONFIG_DTRACE_BSIZE; i++) {
    if (dringbufTemp->name == NULL) {
      dringbufTemp = dringbufTemp->next;
      continue;
    }
    snprintf(printBuf, sizeof(printBuf),
             "%sPC: " FMT_WORD " %s %s at " FMT_PADDR ", len: %d, data: " FMT_WORD,
             dringbufTemp == dringbufNow ? " ---> " : "      ",
             dringbufTemp->pc,
             dringbufTemp->is_write ? "Write" : "Read ",
             dringbufTemp->name,
             dringbufTemp->addr,
             dringbufTemp->len,
             dringbufTemp->data);
    puts(printBuf);
    dringbufTemp = dringbufTemp->next;
  }
}

static void push_device_access(const char *name, paddr_t addr, int len,
                               word_t data, vaddr_t pc, bool is_write) {
  struct DtraceNode *node = dringbufNow->next;
  node->name = name;
  node->addr = addr;
  node->pc = pc;
  node->data = data;
  node->len = len;
  node->is_write = is_write;
  dringbufNow = node;
}

void push_device_read(const char *name, paddr_t addr, int len, word_t data, vaddr_t pc) {
  push_device_access(name, addr, len, data, pc, false);
}

void push_device_write(const char *name, paddr_t addr, int len, word_t data, vaddr_t pc) {
  push_device_access(name, addr, len, data, pc, true);
}

#endif
