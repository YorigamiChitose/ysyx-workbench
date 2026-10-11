#include <utils.h>
#include <common.h>

#ifdef CONFIG_MTRACE

enum {
  T_READ,
  T_WRITE
};
struct MtraceNode mringbuf[CONFIG_MTRACE_BSIZE];
struct MtraceNode *mringbufNow;

void init_mringbuf(void) {
  mringbufNow = &mringbuf[CONFIG_MTRACE_BSIZE - 1];
  for (int i = 0; i < CONFIG_MTRACE_BSIZE; i++) {
    mringbuf[i].next = &mringbuf[(i + 1) % CONFIG_MTRACE_BSIZE];
    mringbuf[i].type = T_READ;
    mringbuf[i].len = 0;
    mringbuf[i].data = 0;
    mringbuf[i].addr = 0;
  }
}

void display_mringbuf(void) {
  char printBuf[CONFIG_TRACE_PBSIZE];
  char* pPrintBuf = printBuf;
  struct MtraceNode *mringbufTemp = mringbufNow->next;
  for (int i = 0; i < CONFIG_MTRACE_BSIZE; i++) {
    if (mringbufTemp->addr == 0) {mringbufTemp = mringbufTemp->next; continue;};
    size_t remain = sizeof(printBuf);
    int n = snprintf(pPrintBuf, remain, "%s %s at " FMT_WORD ", len: %d ", mringbufTemp == mringbufNow ? " ---> ": "      ", mringbufTemp->type == T_WRITE ? "Write": "Read ", mringbufTemp->addr, mringbufTemp->len);
    if (n < 0 || (size_t)n >= remain) { mringbufTemp = mringbufTemp->next; continue; }
    pPrintBuf += n; remain -= n;
    n = snprintf(pPrintBuf, remain, "data: " FMT_WORD, mringbufTemp->data);
    if (n < 0 || (size_t)n >= remain) { mringbufTemp = mringbufTemp->next; continue; }
    mringbufTemp = mringbufTemp->next;
    puts(printBuf);
    pPrintBuf = printBuf;
  }
}

void push_write(vaddr_t addr, int len, word_t data) {
  mringbufNow->next->addr = addr;
  mringbufNow->next->len = len;
  mringbufNow->next->data = data;
  mringbufNow->next->type = T_WRITE;
  mringbufNow = mringbufNow->next;
}

void push_read(vaddr_t addr, int len, word_t data) {
  mringbufNow->next->addr = addr;
  mringbufNow->next->len = len;
  mringbufNow->next->data = data;
  mringbufNow->next->type = T_READ;
  mringbufNow = mringbufNow->next;
}

#endif