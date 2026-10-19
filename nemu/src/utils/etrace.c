#include <utils.h>
#include <common.h>

#ifdef CONFIG_ETRACE

#define ETRACE_INTR_BIT (MUXDEF(CONFIG_ISA64, 1ull << 63, 1u << 31))

extern uint64_t g_nr_guest_inst;

struct EtraceNode etracebuf[CONFIG_ETRACE_BSIZE];
struct EtraceNode *etracebufNow;

static const char *exception_name(word_t code) {
  switch (code) {
    case 0:  return "inst-addr-misaligned";
    case 1:  return "inst-access-fault";
    case 2:  return "illegal-instruction";
    case 3:  return "breakpoint";
    case 4:  return "load-addr-misaligned";
    case 5:  return "load-access-fault";
    case 6:  return "store-addr-misaligned";
    case 7:  return "store-access-fault";
    case 8:  return "ecall-from-u";
    case 9:  return "ecall-from-s";
    case 11: return "ecall-from-m";
    case 12: return "inst-page-fault";
    case 13: return "load-page-fault";
    case 15: return "store-page-fault";
    default: return "unknown";
  }
}

static const char *interrupt_name(word_t code) {
  switch (code) {
    case 1:  return "supervisor-software";
    case 3:  return "machine-software";
    case 5:  return "supervisor-timer";
    case 7:  return "machine-timer";
    case 9:  return "supervisor-external";
    case 11: return "machine-external";
    default: return "unknown";
  }
}

static char mode_char(uint8_t mode) {
  switch (mode) {
    case 0: return 'U';
    case 1: return 'S';
    case 3: return 'M';
    default: return '?';
  }
}

void init_etrace(void) {
  etracebufNow = &etracebuf[CONFIG_ETRACE_BSIZE - 1];
  for (int i = 0; i < CONFIG_ETRACE_BSIZE; i++) {
    etracebuf[i].next = &etracebuf[(i + 1) % CONFIG_ETRACE_BSIZE];
    etracebuf[i].seq = 0;
    etracebuf[i].epc = 0;
    etracebuf[i].target = 0;
    etracebuf[i].cause = 0;
    etracebuf[i].status = 0;
    etracebuf[i].info = 0;
    etracebuf[i].from_mode = 0;
    etracebuf[i].to_mode = 0;
    etracebuf[i].is_interrupt = false;
    etracebuf[i].valid = false;
  }
}

void display_etrace(void) {
  struct EtraceNode *node = etracebufNow->next;
  for (int i = 0; i < CONFIG_ETRACE_BSIZE; i++) {
    if (!node->valid) {
      node = node->next;
      continue;
    }

    word_t code = node->cause & ~(word_t)ETRACE_INTR_BIT;
    const char *name = node->is_interrupt ? interrupt_name(code) : exception_name(code);
    printf("%s#%-8" PRIu64 " %-9s %-24s cause=" FMT_WORD " @ " FMT_WORD
           " -> " FMT_WORD " (%c->%c) mstatus=" FMT_WORD,
           node == etracebufNow ? " ---> " : "      ",
           node->seq,
           node->is_interrupt ? "interrupt" : "exception",
           name,
           node->cause,
           node->epc,
           node->target,
           mode_char(node->from_mode),
           mode_char(node->to_mode),
           node->status);
    if (node->info != 0) {
      printf(" info=" FMT_WORD, node->info);
    }
    printf("\n");
    node = node->next;
  }
}

void etrace_push(bool is_interrupt, word_t cause, vaddr_t epc, vaddr_t target,
                 word_t status, uint8_t from_mode, uint8_t to_mode, word_t info) {
  struct EtraceNode *node = etracebufNow->next;
  node->seq = g_nr_guest_inst;
  node->epc = epc;
  node->target = target;
  node->cause = cause;
  node->status = status;
  node->info = info;
  node->from_mode = from_mode;
  node->to_mode = to_mode;
  node->is_interrupt = is_interrupt;
  node->valid = true;
  etracebufNow = node;
}

#endif
