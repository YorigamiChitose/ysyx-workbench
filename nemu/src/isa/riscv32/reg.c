/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>
#include "local-include/reg.h"

const char *regs[] = {
  "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
  "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
  "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
  "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

void isa_reg_display() {
    for (int i = 0; i < MUXDEF(CONFIG_RVE, 16, 32) / 4; i++) {
    for (int j = 0; j < 4; j++) {
      printf(ANSI_FG_GREEN "%-4s" ANSI_NONE ": " FMT_WORD "\t", reg_name(i*4+j), gpr(i*4+j));
    }
    printf("\n");
  }
  printf(ANSI_FG_RED "%-7s" ANSI_NONE ": " FMT_WORD "\n", "pc", cpu.pc);
  printf(ANSI_FG_RED "%-7s" ANSI_NONE ": "   "%d"   "\n", "mod", cpu.mode);
  printf(ANSI_FG_RED "%-7s" ANSI_NONE ": " FMT_WORD "\n", "mepc", cpu.csr.mepc);
  printf(ANSI_FG_RED "%-7s" ANSI_NONE ": " FMT_WORD "\n", "mcause", cpu.csr.mcause);
  printf(ANSI_FG_RED "%-7s" ANSI_NONE ": " FMT_WORD "\n", "mstatus", cpu.csr.mstatus);
  printf(ANSI_FG_RED "%-7s" ANSI_NONE ": " FMT_WORD "\n", "mtvec", cpu.csr.mtvec);
}

/* Parse numeric register names "x0".."x31" ('$' is already stripped
 * by the expression parser). Returns -1 on any malformed input.
 */
static int parse_reg_idx(const char *s) {
  if (s[0] != 'x' || s[1] < '0' || s[1] > '9') return -1;
  int idx = 0;
  for (const char *p = s + 1; *p != '\0'; p++) {
    if (*p < '0' || *p > '9') return -1;
    idx = idx * 10 + (*p - '0');
    if (idx >= MUXDEF(CONFIG_RVE, 16, 32)) return -1;
  }
  return idx;
}

word_t isa_reg_str2val(const char *s, bool *success) {
  int idx = parse_reg_idx(s);
  if (idx >= 0) {
    *success = true;
    return gpr(idx);
  }
  for (int i = 0; i < MUXDEF(CONFIG_RVE, 16, 32); i++) {
    if (strcmp(s,regs[i]) == 0) {
      (*success) = true;
      return gpr(i);
    }
  }
  (*success) = false;
  Log("Reg name error, Please retry!\n");
  return 0;
}
