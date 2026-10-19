#include <utils.h>
#include <common.h>

#ifdef CONFIG_FTRACE
enum {
  T_CALL,
  T_RET
};

struct FtraceNode ftraceList[CONFIG_FTRACE_BSIZE];
struct FtraceNode* ftraceHead = ftraceList;
struct FtraceNode* ftraceNow = ftraceList;
struct FuncInfo global = {
  "global",
  0x80000000,
  0
};

/* Fallback for calls/rets whose target is not found in the ELF symbol table:
 * record the trace with an <unknown> tag instead of aborting NEMU.
 * One slot per trace node so each record keeps its own target address.
 */
static struct FuncInfo unknown_func[CONFIG_FTRACE_BSIZE];

static struct FuncInfo *get_unknown(paddr_t dstAddr) {
  int idx = (int)(ftraceNow - ftraceList);
  unknown_func[idx].addr = dstAddr;
  unknown_func[idx].size = 0;
  strcpy(unknown_func[idx].name, "<unknown>");
  return &unknown_func[idx];
}


void init_ftrace_node(void) {
  struct FtraceNode* ftraceTemp = ftraceHead;
  ftraceTemp->srcFunc = &global;
  for (int i = 1; i < CONFIG_FTRACE_BSIZE; i++)
  {
    ftraceTemp->next = &ftraceList[i];
    ftraceTemp = ftraceTemp->next;
  }
  ftraceTemp->next = NULL;
}

void trace_func_call(paddr_t pcAddr, paddr_t dstAddr) {
  if (ftraceNow->next == NULL) {
    return;
  }
  for (int i = 0; i < allFucn.num; i++) {
    if (allFucn.funcInfo[i].addr == dstAddr) {
      ftraceNow->dstFunc = &(allFucn.funcInfo[i]);
      ftraceNow->next->srcFunc = &(allFucn.funcInfo[i]);
      ftraceNow->next->callDepth = ftraceNow->callDepth + 1;
      ftraceNow->type = T_CALL;
      ftraceNow->pcAddr = pcAddr;
      ftraceNow = ftraceNow->next;
      return;
    }
  }
  struct FuncInfo *unk = get_unknown(dstAddr);
  ftraceNow->dstFunc = unk;
  ftraceNow->next->srcFunc = unk;
  ftraceNow->next->callDepth = ftraceNow->callDepth + 1;
  ftraceNow->type = T_CALL;
  ftraceNow->pcAddr = pcAddr;
  ftraceNow = ftraceNow->next;
}

void trace_func_ret(paddr_t pcAddr, paddr_t dstAddr) {
  if (ftraceNow->next == NULL) {
    return;
  }
  for (int i = 0; i < allFucn.num; i++) {
    if ((allFucn.funcInfo[i].addr + allFucn.funcInfo[i].size) >= dstAddr && dstAddr >= allFucn.funcInfo[i].addr) {
      ftraceNow->dstFunc = &(allFucn.funcInfo[i]);
      ftraceNow->next->srcFunc = &(allFucn.funcInfo[i]);
      if (ftraceNow->callDepth > 0) {
        ftraceNow->callDepth -= 1;
      }
      ftraceNow->next->callDepth = ftraceNow->callDepth;
      ftraceNow->type = T_RET;
      ftraceNow->pcAddr = pcAddr;
      ftraceNow = ftraceNow->next;
      return;
    }
  }
  struct FuncInfo *unk = get_unknown(dstAddr);
  ftraceNow->dstFunc = unk;
  ftraceNow->next->srcFunc = unk;
  if (ftraceNow->callDepth > 0) {
    ftraceNow->callDepth -= 1;
  }
  ftraceNow->next->callDepth = ftraceNow->callDepth;
  ftraceNow->type = T_RET;
  ftraceNow->pcAddr = pcAddr;
  ftraceNow = ftraceNow->next;
}

void trace_func_end(void) {
  if (ftraceNow->next == NULL) {
    return;
  }
  if (ftraceNow->callDepth > 0) {
    ftraceNow->callDepth -= 1;
  }
  ftraceNow->dstFunc = &global;
  ftraceNow->type = T_RET;
  ftraceNow->pcAddr = 0x80000000;
  ftraceNow = ftraceNow->next;
}

void display_call(void) {
  struct FtraceNode* ftraceTemp = ftraceHead;
  while (ftraceTemp != ftraceNow) {
    printf(
      FMT_PADDR ": %*s%s [" ANSI_FG_GREEN "%s" ANSI_FG_YELLOW "@" ANSI_NONE FMT_PADDR "] -> [" ANSI_FG_GREEN "%s" ANSI_FG_YELLOW "@" ANSI_NONE FMT_PADDR "]\n",
      ftraceTemp->pcAddr, 
      ftraceTemp->callDepth, "", 
      ftraceTemp->type == T_CALL ? "call": ANSI_FG_RED "ret" ANSI_NONE " ", 
      ftraceTemp->srcFunc->name, 
      ftraceTemp->srcFunc->addr,
      ftraceTemp->dstFunc->name, 
      ftraceTemp->dstFunc->addr      
    );
    ftraceTemp = ftraceTemp->next;
  }
}
#endif