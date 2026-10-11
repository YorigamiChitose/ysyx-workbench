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

#ifndef __SDB_H__
#define __SDB_H__

#include <common.h>

typedef struct token {
  int type;
  char str[CONFIG_EXPR_TOKENSTR_SIZE];
} Token;

#ifdef CONFIG_WATCH_POINT
typedef struct watchpoint {
  int NO;
  struct watchpoint *next;
  word_t last_value;
  char expression[CONFIG_WP_EXPR_SIZE];
  Token tokens[CONFIG_EXPR_TOKENS_SIZE];
  int nr_token;
} WP;
bool is_change(void);
void check_wp(void);
WP* new_wp(char* expression, bool* success);
void free_wp(int NO, bool* success);
#endif

word_t expr(char *e, bool *success);
bool expr_compile(char *e, Token *out_tokens, int *out_nr);
word_t expr_eval_compiled(Token *cached, int nr, bool *success);

#endif
