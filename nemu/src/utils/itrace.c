#include <utils.h>
#include <common.h>

#ifdef CONFIG_ITRACE

void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);

struct ItraceNode iringbuf[CONFIG_ITRACE_BSIZE];
struct ItraceNode *iringbufNow;

void init_iringbuf(void) {
  iringbufNow = &iringbuf[CONFIG_ITRACE_BSIZE - 1];
  for (int i = 0; i < CONFIG_ITRACE_BSIZE; i++) {
    iringbuf[i].next = &iringbuf[(i + 1) % CONFIG_ITRACE_BSIZE];
    iringbuf[i].inst = 0;
    iringbuf[i].pc = 0;
  }
}

void display_iringbuf(void) {
  char printBuf[CONFIG_TRACE_PBSIZE];
  char* pPrintBuf = printBuf;
  struct ItraceNode *iringbufTemp = iringbufNow->next;
  for (int i = 0; i < CONFIG_ITRACE_BSIZE; i++) {
    if (iringbufTemp->pc == 0) {iringbufTemp = iringbufTemp->next; continue;};
    size_t remain = sizeof(printBuf);
    int n = snprintf(pPrintBuf, remain, "%sPC: " FMT_PADDR ": ",
                     iringbufTemp == iringbufNow ? " ---> ": "      ", iringbufTemp->pc);
    if (n < 0 || (size_t)n >= remain) { break; }
    pPrintBuf += n; remain -= n;
    for (int i = 3; i >= 0; i--)
    {
      n = snprintf(pPrintBuf, remain, "%02x ", ((uint8_t*)&iringbufTemp->inst)[i]);
      if (n < 0 || (size_t)n >= remain) { break; }
      pPrintBuf += n; remain -= n;
    }
    disassemble(pPrintBuf, remain, iringbufTemp->pc, (uint8_t*)&iringbufTemp->inst, 4);
    iringbufTemp = iringbufTemp->next;
    puts(printBuf);
    pPrintBuf = printBuf;
  }
}

void push_inst(paddr_t pc, uint32_t inst) {
  iringbufNow->next->inst = inst;
  iringbufNow->next->pc = pc;
  iringbufNow = iringbufNow->next;
}

#endif