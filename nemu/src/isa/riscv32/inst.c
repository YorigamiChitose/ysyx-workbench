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

#include "local-include/reg.h"
#include <cpu/cpu.h>
#include <cpu/ifetch.h>
#include <cpu/decode.h>

IFDEF(CONFIG_ITRACE, void push_inst(paddr_t pc, uint32_t inst));
IFDEF(CONFIG_FTRACE, void trace_func_call(paddr_t pcAddr, paddr_t dstAddr));
IFDEF(CONFIG_FTRACE, void trace_func_ret(paddr_t pcAddr, paddr_t dstAddr));
IFDEF(CONFIG_FTRACE, void trace_func_end(void));

#define R(i) gpr(i)
#define Mr vaddr_read
#define Mw vaddr_write

enum {
  CSR_MEPC    = 0x341,
  CSR_MCAUSE  = 0x342,
  CSR_MSTATUS = 0x300,
  CSR_MTVEC   = 0x305
};

enum {
  TYPE_I, TYPE_U, TYPE_S,
  TYPE_R, TYPE_J, TYPE_B,
  TYPE_CSR,  // csrrw/csrrs/csrrc: src1 = rs1, imm = CSR number (zero-extended)
  TYPE_CSRI, // csrrwi/csrrsi/csrrci: src1 = uimm, imm = CSR number
  TYPE_N, // none
};

#define S32(i) ((int32_t)i)
#define S64(i) ((int64_t)i)
#define U32(i) ((uint32_t)i)
#define U64(i) ((uint64_t)i)
#define U(i) ((word_t)i)
#define S(i) ((sword_t)i)
#define CSR(i) *get_csr_register(i)
#define MODE (cpu.mode)
#define ECALL switch (MODE) { \
                case MACHINE_MODE_U: s->dnpc = isa_raise_intr(0x08, s->pc); break; \
                case MACHINE_MODE_S: s->dnpc = isa_raise_intr(0x09, s->pc); break; \
                case MACHINE_MODE_M: s->dnpc = isa_raise_intr(0x0B, s->pc); break; \
              }
#define MRET cpu.csr.mstatus = (cpu.csr.mstatus & ~(1 << 3)) | ((cpu.csr.mstatus & (1 << 7)) >> 4); \
          cpu.csr.mstatus |= (1 << 7); \
          cpu.mode = (cpu.csr.mstatus >> 11) & 0x03; \
          cpu.csr.mstatus &= ~((1u << 11) | (1u << 12));
#define XLEN_H MUXDEF(CONFIG_RV64, 32, 16)
#define XLEN MUXDEF(CONFIG_RV64, 64, 32)
#define src1R() do { *src1 = R(rs1); } while (0)
#define src2R() do { *src2 = R(rs2); } while (0)
#define immI() do { *imm = SEXT(BITS(i, 31, 20), 12); } while(0)
#define csrNo() do { *imm = BITS(i, 31, 20); } while(0)
#define immU() do { *imm = SEXT(BITS(i, 31, 12), 20) << 12; } while(0)
#define immS() do { *imm = (SEXT(BITS(i, 31, 25), 7) << 5) | BITS(i, 11, 7); } while(0)
#define immJ() do { *imm = SEXT(((BITS(i, 31, 31) << 20) | (BITS(i, 30, 21) << 1) | (BITS(i, 20, 20) << 11) | (BITS(i, 19, 12) << 12)), 21); } while(0)
#define immB() do { *imm = SEXT(((BITS(i, 31, 31) << 12) | (BITS(i, 30, 25) << 5) | (BITS(i, 11, 8) << 1) | (BITS(i, 7, 7) << 11) ), 13); } while(0)

static vaddr_t *get_csr_register(word_t imm) {
  switch (imm)
  {
    case CSR_MEPC: return &(cpu.csr.mepc);
    case CSR_MCAUSE: return &(cpu.csr.mcause);
    case CSR_MSTATUS: return &(cpu.csr.mstatus);
    case CSR_MTVEC: return &(cpu.csr.mtvec);
    default: panic("Unsupported CSR: 0x%03x", (unsigned)(imm & 0xfff));
  }
}

/* RV32M division: per RISC-V spec, division by zero does not trap,
 * and signed overflow (MIN / -1) saturates. Check before using the
 * host '/' and '%' to avoid a host SIGFPE.
 */
static word_t div_s(sword_t a, sword_t b) {
  if (b == 0) return (word_t)-1;
  sword_t min = (sword_t)((word_t)1 << (XLEN - 1));
  if (a == min && b == -1) return (word_t)a;
  return (word_t)(a / b);
}

static word_t rem_s(sword_t a, sword_t b) {
  if (b == 0) return (word_t)a;
  sword_t min = (sword_t)((word_t)1 << (XLEN - 1));
  if (a == min && b == -1) return 0;
  return (word_t)(a % b);
}

static word_t div_u(word_t a, word_t b) {
  return b == 0 ? (word_t)-1 : a / b;
}

static word_t rem_u(word_t a, word_t b) {
  return b == 0 ? a : a % b;
}

static void decode_operand(Decode *s, int *rd, word_t *src1, word_t *src2, word_t *imm, int type) {
  uint32_t i = s->isa.inst;
  int rs1 = BITS(i, 19, 15);
  int rs2 = BITS(i, 24, 20);
  *rd     = BITS(i, 11, 7);
  switch (type) {
    case TYPE_I: src1R();          immI(); break;
    case TYPE_U:                   immU(); break;
    case TYPE_S: src1R(); src2R(); immS(); break;
    case TYPE_R: src1R(); src2R();         break;
    case TYPE_J:                   immJ(); break;
    case TYPE_B: src1R(); src2R(); immB(); break;
    case TYPE_CSR:  src1R();                  csrNo(); break;
    case TYPE_CSRI: *src1 = BITS(i, 19, 15);  csrNo(); break;
  }
}



static int decode_exec(Decode *s) {
  s->dnpc = s->snpc;
  IFDEF(CONFIG_FTRACE, int rs1 = BITS(s->isa.inst, 19, 15));

#define INSTPAT_INST(s) ((s)->isa.inst)
#define INSTPAT_MATCH(s, name, type, ... /* execute body */ ) { \
  int rd = 0; \
  word_t src1 = 0, src2 = 0, imm = 0; \
  decode_operand(s, &rd, &src1, &src2, &imm, concat(TYPE_, type)); \
  __VA_ARGS__ ; \
}

  INSTPAT_START();
  // RV32I
  INSTPAT("??????? ????? ????? 000 ????? 00000 11", lb     , I, R(rd) = SEXT(BITS(Mr(src1 + imm, 1), 7, 0), 8));
  INSTPAT("??????? ????? ????? 001 ????? 00000 11", lh     , I, R(rd) = SEXT(BITS(Mr(src1 + imm, 2), 15, 0), 16));
  INSTPAT("??????? ????? ????? 010 ????? 00000 11", lw     , I, R(rd) = SEXT(BITS(Mr(src1 + imm, 4), 31, 0), 32));
  INSTPAT("??????? ????? ????? 100 ????? 00000 11", lbu    , I, R(rd) = Mr(src1 + imm, 1));
  INSTPAT("??????? ????? ????? 101 ????? 00000 11", lhu    , I, R(rd) = BITS(Mr(src1 + imm, 2), 15, 0));
  INSTPAT("??????? ????? ????? 000 ????? 01000 11", sb     , S, Mw(src1 + imm, 1, src2));
  INSTPAT("??????? ????? ????? 001 ????? 01000 11", sh     , S, Mw(src1 + imm, 2, BITS(src2, 15, 0)));
  INSTPAT("??????? ????? ????? 010 ????? 01000 11", sw     , S, Mw(src1 + imm, 4, BITS(src2, 31, 0)));
  INSTPAT("0000000 ????? ????? 000 ????? 01100 11", add    , R, R(rd) = src1 + src2);
  INSTPAT("??????? ????? ????? 000 ????? 00100 11", addi   , I, R(rd) = src1 + imm);
  INSTPAT("0100000 ????? ????? 000 ????? 01100 11", sub    , R, R(rd) = src1 - src2);
  INSTPAT("??????? ????? ????? ??? ????? 01101 11", lui    , U, R(rd) = imm);
  INSTPAT("??????? ????? ????? ??? ????? 00101 11", auipc  , U, R(rd) = s->pc + imm);
  INSTPAT("0000000 ????? ????? 100 ????? 01100 11", xor    , R, R(rd) = src1 ^ src2);
  INSTPAT("??????? ????? ????? 100 ????? 00100 11", xori   , I, R(rd) = src1 ^ imm);
  INSTPAT("0000000 ????? ????? 110 ????? 01100 11", or     , R, R(rd) = src1 | src2);
  INSTPAT("??????? ????? ????? 110 ????? 00100 11", ori    , I, R(rd) = src1 | imm);
  INSTPAT("0000000 ????? ????? 111 ????? 01100 11", and    , R, R(rd) = src1 & src2);
  INSTPAT("??????? ????? ????? 111 ????? 00100 11", andi   , I, R(rd) = src1 & imm);
  INSTPAT(MUXDEF(CONFIG_RV64, "000000? ????? ????? 001 ????? 00100 11",
                              "0000000 ????? ????? 001 ????? 00100 11"),
          slli, I, R(rd) = (U(src1) << BITS(imm, MUXDEF(CONFIG_RV64, 5, 4), 0)));
  INSTPAT("0000000 ????? ????? 001 ????? 01100 11", sll    , R, R(rd) = src1 << BITS(src2, MUXDEF(CONFIG_RV64, 5, 4), 0));
  INSTPAT("0000000 ????? ????? 101 ????? 01100 11", srl    , R, R(rd) = U(src1) >> BITS(src2, MUXDEF(CONFIG_RV64, 5, 4), 0));
  INSTPAT(MUXDEF(CONFIG_RV64, "000000? ????? ????? 101 ????? 00100 11",
                              "0000000 ????? ????? 101 ????? 00100 11"),
          srli, I, R(rd) = (U(src1) >> BITS(imm, MUXDEF(CONFIG_RV64, 5, 4), 0)));
  INSTPAT("0100000 ????? ????? 101 ????? 01100 11", sra    , R, R(rd) = S(src1) >> BITS(src2, MUXDEF(CONFIG_RV64, 5, 4), 0));
  INSTPAT(MUXDEF(CONFIG_RV64, "010000? ????? ????? 101 ????? 00100 11",
                              "0100000 ????? ????? 101 ????? 00100 11"),
          srai, I, R(rd) = (S(src1) >> BITS(imm, MUXDEF(CONFIG_RV64, 5, 4), 0)));
  INSTPAT("0000000 ????? ????? 010 ????? 01100 11", slt    , R, R(rd) = (S(src1) < S(src2)));
  INSTPAT("??????? ????? ????? 010 ????? 00100 11", slti   , I, R(rd) = (S(src1) < S(imm)));
  INSTPAT("0000000 ????? ????? 011 ????? 01100 11", sltu   , R, R(rd) = (src1 < src2));
  INSTPAT("??????? ????? ????? 011 ????? 00100 11", sltiu  , I, R(rd) = (src1 < imm));
  INSTPAT("??????? ????? ????? 000 ????? 11000 11", beq    , B, s->dnpc = ((src1 == src2) ? s->pc + imm : s->snpc));
  INSTPAT("??????? ????? ????? 001 ????? 11000 11", bne    , B, s->dnpc = ((src1 != src2) ? s->pc + imm : s->snpc));
  INSTPAT("??????? ????? ????? 100 ????? 11000 11", blt    , B, s->dnpc = (S(src1) < S(src2)) ? s->pc + imm : s->snpc);
  INSTPAT("??????? ????? ????? 101 ????? 11000 11", bge    , B, s->dnpc = (S(src1) >= S(src2)) ? s->pc + imm : s->snpc);
  INSTPAT("??????? ????? ????? 110 ????? 11000 11", bltu   , B, s->dnpc = (src1 < src2) ? s->pc + imm : s->snpc);
  INSTPAT("??????? ????? ????? 111 ????? 11000 11", bgeu   , B, s->dnpc = (src1 >= src2) ? s->pc + imm : s->snpc);
  INSTPAT("??????? ????? ????? ??? ????? 11011 11", jal    , J, R(rd) = s->snpc; s->dnpc = imm + s->pc; 
    IFDEF(CONFIG_FTRACE, if (rd == 1) {
      trace_func_call(s->pc, s->dnpc);
  }));
  INSTPAT("??????? ????? ????? 000 ????? 11001 11", jalr   , I, R(rd) = s->snpc; s->dnpc = (src1 + imm) & (~1);
    IFDEF(CONFIG_FTRACE, if (rd == 1) {
      trace_func_call(s->pc, s->dnpc);
    } else if (rd == 0 && rs1 == 1){
      trace_func_ret(s->pc, s->dnpc);
  }));
  INSTPAT("0000000 00000 00000 000 00000 11100 11", ecall  , I, ECALL);
  INSTPAT("0000000 00001 00000 000 00000 11100 11", ebreak , N, NEMUTRAP(s->pc, R(10)); IFDEF(CONFIG_FTRACE, trace_func_end());); // R(10) is $a0
  // RV32M
  INSTPAT("0000001 ????? ????? 000 ????? 01100 11", mul    , R, R(rd) = src1 * src2);
  INSTPAT("0000001 ????? ????? 001 ????? 01100 11", mulh   , R, R(rd) = MUXDEF(CONFIG_RV64, S64((S(src1) >> XLEN_H) * (S(src2) >> XLEN_H)), (SEXT(src1, XLEN) * SEXT(src2, XLEN)) >> XLEN));
  INSTPAT("0000001 ????? ????? 011 ????? 01100 11", mulhu  , R, R(rd) = MUXDEF(CONFIG_RV64, U64((U(src1) >> XLEN_H) * (U(src2) >> XLEN_H)), (U64(src1)        * U64(src2))        >> XLEN));
  INSTPAT("0000001 ????? ????? 010 ????? 01100 11", mulhsu , R, R(rd) = MUXDEF(CONFIG_RV64, U64((S(src1) >> XLEN_H) * (U(src2) >> XLEN_H)), (SEXT(src1, XLEN) * U64(src2))        >> XLEN));
  INSTPAT("0000001 ????? ????? 100 ????? 01100 11", div    , R, R(rd) = div_s(S(src1), S(src2)));
  INSTPAT("0000001 ????? ????? 101 ????? 01100 11", divu   , R, R(rd) = div_u(   src1,    src2));
  INSTPAT("0000001 ????? ????? 110 ????? 01100 11", rem    , R, R(rd) = rem_s(S(src1), S(src2)));
  INSTPAT("0000001 ????? ????? 111 ????? 01100 11", remu   , R, R(rd) = rem_u(   src1,    src2));
  // RV Zicsr
  INSTPAT("??????? ????? ????? 001 ????? 11100 11", csrrw  , CSR,  R(rd) = CSR(imm); CSR(imm) = src1);
  INSTPAT("??????? ????? ????? 010 ????? 11100 11", csrrs  , CSR,  R(rd) = CSR(imm); CSR(imm) |= src1);
  INSTPAT("??????? ????? ????? 011 ????? 11100 11", csrrc  , CSR,  R(rd) = CSR(imm); CSR(imm) &= ~src1);
  INSTPAT("??????? ????? ????? 101 ????? 11100 11", csrrwi , CSRI, R(rd) = CSR(imm); CSR(imm) = src1);
  INSTPAT("??????? ????? ????? 110 ????? 11100 11", csrrsi , CSRI, R(rd) = CSR(imm); CSR(imm) |= src1);
  INSTPAT("??????? ????? ????? 111 ????? 11100 11", csrrci , CSRI, R(rd) = CSR(imm); CSR(imm) &= ~src1);
  INSTPAT("0011000 00010 00000 000 00000 11100 11", mret   , N, s->dnpc = CSR(CSR_MEPC); MRET);
  // RV64I
  INSTPAT("??????? ????? ????? 011 ????? 00000 11", ld     , I, R(rd) = Mr(src1 + imm, 8));
  INSTPAT("??????? ????? ????? 110 ????? 00000 11", lwu    , I, R(rd) = BITS(Mr(src1 + imm, 4), 31, 0));
  INSTPAT("??????? ????? ????? 011 ????? 01000 11", sd     , S, Mw(src1 + imm, 8, src2));
  INSTPAT("??????? ????? ????? 000 ????? 00110 11", addiw  , I, R(rd) = SEXT(BITS(src1 + imm, 31, 0), 32));
  INSTPAT("0000000 ????? ????? 000 ????? 01110 11", addw   , R, R(rd) = SEXT(BITS(src1 + src2, 31, 0), 32));
  INSTPAT("0100000 ????? ????? 000 ????? 01110 11", subw   , R, R(rd) = SEXT(BITS(src1 - src2, 31, 0), 32));
  INSTPAT("0000000 ????? ????? 001 ????? 00110 11", slliw  , I, R(rd) = SEXT(BITS(BITS(src1, 31, 0) << BITS(imm, 5, 0), 31, 0), 32));
  INSTPAT("0000000 ????? ????? 001 ????? 01110 11", sllw   , R, R(rd) = SEXT(BITS(BITS(src1, 31, 0) << BITS(src2, 4, 0), 31, 0), 32));
  INSTPAT("0000000 ????? ????? 101 ????? 00110 11", srliw  , I, R(rd) = SEXT(BITS(BITS(src1, 31, 0) >> BITS(imm, 5, 0), 31, 0), 32));
  INSTPAT("0000000 ????? ????? 101 ????? 01110 11", srlw   , R, R(rd) = SEXT(BITS(BITS(src1, 31, 0) >> BITS(src2, 4, 0), 31, 0), 32));
  INSTPAT("0100000 ????? ????? 101 ????? 00110 11", sraiw  , I, R(rd) = SEXT(BITS(S32(BITS(src1, 31, 0)) >> BITS(imm, 4, 0), 31, 0), 32));
  INSTPAT("0100000 ????? ????? 101 ????? 01110 11", sraw   , R, R(rd) = SEXT(BITS(S32(BITS(src1, 31, 0)) >> S32(BITS(src2, 4, 0)), 31, 0), 32));
  // RV64M
  INSTPAT("0000001 ????? ????? 000 ????? 01110 11", mulw   , R, R(rd) = SEXT(S32(BITS(src1, 31, 0)) * S32(BITS(src2, 31, 0)), 32));
  INSTPAT("0000001 ????? ????? 100 ????? 01110 11", divw   , R, R(rd) = SEXT(S32(BITS(src1, 31, 0)) / S32(BITS(src2, 31, 0)), 32));
  INSTPAT("0000001 ????? ????? 110 ????? 01110 11", remw   , R, R(rd) = SEXT(S32(BITS(src1, 31, 0)) % S32(BITS(src2, 31, 0)), 32));
  INSTPAT("0000001 ????? ????? 101 ????? 01110 11", divuw  , R, R(rd) = SEXT(BITS(src1, 31, 0) / BITS(src2, 31, 0), 32));
  INSTPAT("0000001 ????? ????? 111 ????? 01110 11", remuw  , R, R(rd) = SEXT(BITS(src1, 31, 0) % BITS(src2, 31, 0), 32));
  INSTPAT("??????? ????? ????? ??? ????? ????? ??", inv    , N, INV(s->pc));
  INSTPAT_END();

  R(0) = 0; // reset $zero to 0

  return 0;
}

int isa_exec_once(Decode *s) {
  s->isa.inst = inst_fetch(&s->snpc, 4);
  IFDEF(CONFIG_ITRACE, push_inst(s->pc, s->isa.inst));
  return decode_exec(s);
}
