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

#include "sdb.h"

#ifdef CONFIG_WATCH_POINT

static WP wp_pool[CONFIG_WP_SIZE] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < CONFIG_WP_SIZE; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == CONFIG_WP_SIZE - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}
WP* new_wp(char* expression, bool* success) {
  if (free_ == NULL) {
    *success = false;
    return NULL;
  }
  Token compiled[CONFIG_EXPR_TOKENS_SIZE];
  int compiled_nr = 0;
  if (!expr_compile(expression, compiled, &compiled_nr)) {
    Log("Error! Please retry.");
    *success = false;
    return NULL;
  }
  bool flag = true;
  word_t expr_value = expr_eval_compiled(compiled, compiled_nr, &flag);
  if (!flag) {
    Log("Error! Please retry.");
    *success = false;
    return NULL;
  }
  WP* new_wp_p;
  if (head == NULL) {
    head = free_;
    new_wp_p = head;
    free_ = free_->next;
    head->next = NULL;
  } 
  else {
    new_wp_p = free_;
    free_ = free_->next;
    new_wp_p->next = head;
    head = new_wp_p;
  }
  strcpy(new_wp_p->expression, expression);
  memcpy(new_wp_p->tokens, compiled, compiled_nr * sizeof(Token));
  new_wp_p->nr_token = compiled_nr;
  new_wp_p->last_value = expr_value;
  *success = true;
  return new_wp_p;
}

void check_wp(void) {
  WP* temp = head;
  while (temp != NULL) {
    printf("watchpoint NO:%d last_value:" FMT_WORD " expr:%s\n",temp->NO, temp->last_value, temp->expression);
    temp = temp->next;
  }
}

void free_wp(int NO, bool* success) {
  WP* temp = NULL;
  WP* head_temp = head;
  while(head_temp != NULL) {
    if (head_temp->next != NULL) {
      if (head_temp->next->NO == NO) {
        temp = head_temp->next;
        head_temp->next = head_temp->next->next;
        temp->next = free_;
        free_ = temp;
        *success = true;
        return;
      }
      else if (head_temp == head && head_temp->NO == NO) {
        temp = head_temp->next;
        head_temp->next = free_;
        free_ = head_temp;
        head = temp;
        *success = true;
        return;
      }
    }
    else if (head_temp->next == NULL) {
      if (head == head_temp && head_temp->NO == NO) {
        head_temp->next = free_;
        free_ = head_temp;
        head = NULL;  
        *success = true;
        return;
      }
    }
    head_temp = head_temp->next;
  }
  *success = false;
}

bool is_change(void) {
  bool changed = false;
  WP* temp = head;
  while(temp != NULL) {
    bool success = true;
    word_t value_now = expr_eval_compiled(temp->tokens, temp->nr_token, &success);
    if (success && value_now != temp->last_value) {
      printf("watchpoint NO:%d last_value:" FMT_WORD " value_now:" FMT_WORD " expr:%s\n",temp->NO,temp->last_value,value_now,temp->expression);
      temp->last_value = value_now;
      changed = true;
    }
    temp = temp->next;
  }
  if (changed) {
    Log("The value of watchpoints have changed.");
  }
  return changed;
}
#endif
