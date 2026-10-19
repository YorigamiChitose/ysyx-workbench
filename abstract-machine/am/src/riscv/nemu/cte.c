#include <am.h>
#include <riscv/riscv.h>
#include <klib.h>

static Context* (*user_handler)(Event, Context*) = NULL;

Context* __am_irq_handle(Context *c) {
  if (user_handler) {
    Event ev = {0};
    switch (c->mcause) {
      case ET_ECALL_FROM_MMODE:
        ev.event = c->GPR1 == (uintptr_t)-1 ? EVENT_YIELD : EVENT_SYSCALL;
        c->mepc += 4;
        break;
      default:
        printf("Unhandled trap: mcause = %lu\n", (unsigned long)c->mcause);
        ev.event = EVENT_ERROR;
        break;
    }

    c = user_handler(ev, c);
    assert(c != NULL);
  }

  return c;
}

extern void __am_asm_trap(void);

bool cte_init(Context*(*handler)(Event, Context*)) {
  // initialize exception entry
  asm volatile("csrw mtvec, %0" : : "r"(__am_asm_trap));

  // register event handler
  user_handler = handler;

  return true;
}

Context *kcontext(Area kstack, void (*entry)(void *), void *arg) {
  Context *context = (Context *)kstack.end - 1;
  memset(context, 0, sizeof(*context));
  context->mstatus = MSTATUS_SXL | MSTATUS_UXL | 0x1800;
  context->mepc = (uintptr_t)entry;
  context->gpr[10] = (uintptr_t)arg;
  return context;
}

void yield() {
#ifdef __riscv_e
  asm volatile("li a5, -1; ecall");
#else
  asm volatile("li a7, -1; ecall");
#endif
}

bool ienabled() {
  return false;
}

void iset(bool enable) {
}
