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

IFDEF(CONFIG_ETRACE, void etrace_push(bool is_interrupt, word_t cause, vaddr_t epc, vaddr_t target, word_t status, uint8_t from_mode, uint8_t to_mode, word_t info));
IFDEF(CONFIG_ETRACE, void etrace_push_return(vaddr_t pc, vaddr_t target, word_t status, uint8_t from_mode, uint8_t to_mode));

word_t isa_raise_intr(word_t NO, vaddr_t epc) {
  IFDEF(CONFIG_ETRACE, word_t status_before = cpu.csr.mstatus);
  IFDEF(CONFIG_ETRACE, uint8_t mode_before = cpu.mode);
  cpu.csr.mepc = epc;
  cpu.csr.mcause = NO;
  cpu.csr.mstatus = (cpu.csr.mstatus & ~(1 << 7)) | ((cpu.csr.mstatus & (1 << 3)) << 4); // MIE -> MPIE
  cpu.csr.mstatus &= ~(1 << 3); // 0 -> MIE
  cpu.csr.mstatus &= ~((1 << 11) + (1 << 12)); // 00 -> MPP
  cpu.csr.mstatus |= cpu.mode << 11; // mode -> MPP
  cpu.mode = 0x03;
  IFDEF(CONFIG_ETRACE, {
    bool is_interrupt = (NO >> MUXDEF(CONFIG_ISA64, 63, 31)) != 0;
    word_t info = 0;
    if (!is_interrupt && NO >= 0x08 && NO <= 0x0B) {
      info = cpu.gpr[MUXDEF(CONFIG_RVE, 15, 17)];
    }
    etrace_push(is_interrupt, NO, epc, cpu.csr.mtvec, status_before, mode_before, cpu.mode, info);
  });
  return cpu.csr.mtvec;
}

word_t isa_query_intr() {
  return INTR_EMPTY;
}
