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
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"
#include "../../../include/memory/paddr.h"

#define LIST_NUM 3
#define add_cnt(x) (x % LIST_NUM ? 1 : 0)
#define addr_cnt(x) (x / LIST_NUM + add_cnt(x))

static int is_batch_mode = false;

void init_regex();
IFDEF(CONFIG_WATCH_POINT, void init_wp_pool());
IFDEF(CONFIG_ITRACE, void display_iringbuf(void));
IFDEF(CONFIG_MTRACE, void display_mringbuf(void));
IFDEF(CONFIG_DTRACE, void display_dringbuf(void));
IFDEF(CONFIG_ETRACE, void display_etrace(void));
IFDEF(CONFIG_FTRACE, void display_call(void));
IFDEF(CONFIG_DIFFTEST, void isa_difftest_attach(void));

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;
  HIST_ENTRY *last_line = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  } else if (line_read && (history_length > 0)) {
    last_line = history_get(history_length);
    free(line_read);
    line_read = malloc((strlen(last_line->line) + 1) * sizeof(char));
    memcpy(line_read, last_line->line, (strlen(last_line->line) + 1) * sizeof(char));
  }

  return line_read;
}

static int cmd_help(char *args);
static int cmd_c(char *args);
static int cmd_q(char *args);
static int cmd_p(char *args);
static int cmd_w(char *args);
static int cmd_d(char *args);
static int cmd_x(char *args);
static int cmd_info(char *args);
static int cmd_trace(char *args);
static int cmd_si(char *args);
static int cmd_attach(char *args);
static int cmd_detach(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help",   "Usage: help\t[str]\t\t - Display information about all supported commands.", cmd_help},
  { "c",      "Usage: c\t\t\t - Continue the execution of the program.", cmd_c},
  { "q",      "Usage: q\t\t\t - Exit NEMU.", cmd_q},
  { "p",      "Usage: p\t[str]\t\t - Calculate.", cmd_p},
  { "w",      "Usage: w\t[str]\t\t - Set a new watchpoint.", cmd_w},
  { "d",      "Usage: d\t[int]\t\t - Delete a watchpoint.", cmd_d},
  { "x",      "Usage: x\t[int] [str]\t - Scan the memory.", cmd_x},
  { "info",   "Usage: info\tr | w\t\t - Get the info.", cmd_info},
  { "trace",  "Usage: trace\t[i|m|d|e|f]\t - Display trace information.", cmd_trace},
  { "si",     "Usage: si\t[int]\t\t - Run step by step.", cmd_si},
  { "attach", "Usage: attach\t\t\t - Enable difftest.", cmd_attach},
  { "detach", "Usage: detach\t\t\t - Disable difftest.", cmd_detach},
};

#define SDB_MAX_ARGS 8

/* Split the raw argument string in place into space-separated tokens.
 * Returns the token count (capped at max); argv entries point into args.
 */
static int split_args(char *args, char **argv, int max) {
  int argc = 0;
  char *p = args;
  while (argc < max) {
    while (*p == ' ') p++;
    if (*p == '\0') break;
    argv[argc++] = p;
    while (*p != '\0' && *p != ' ') p++;
    if (*p == ' ') {
      *p = '\0';
      p++;
    }
  }
  return argc;
}

char *get_arguments(char *args, int num) {
  if (args == NULL || num < 1) {
    return NULL;
  }
  char *argv[SDB_MAX_ARGS];
  if (split_args(args, argv, SDB_MAX_ARGS) < num) {
    return NULL;
  }
  return argv[num - 1];
}

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  char *arg = strtok(NULL, " ");
  int i;
  if (arg == NULL) {
    for (i = 0; i < NR_CMD; i ++) {
      printf("%-6s- %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%-6s- %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Error! Unknown command '%s'\n", arg);
  }
  return 0;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_q(char *args) {
  if (nemu_state.state == NEMU_ABORT) {
    nemu_state.halt_ret = 1;
  }
  if (nemu_state.halt_ret == 0) {
    nemu_state.state = NEMU_QUIT;
  }
  return -1;
}

static int cmd_p(char *args) {
  if (args == NULL) {
    printf("Error, please retry!\n");
    return 0;
  }
  char *arg = args;
  bool flag = true;
  word_t answer = expr(arg, &flag);
  if (flag) {
    printf("answer = " FMT_WORD "\n", answer);
  } else {
    printf("Error! please retry!\n");
  }
  return 0;
}

static int cmd_d(char *args) {
#ifdef CONFIG_WATCH_POINT
  if (args == NULL) {
    printf("Error, please retry!\n");
    return 0;
  }
  char *arg = args;
  bool flag = true;
  word_t answer = expr(arg, &flag);
  if (flag) {
    bool success;
    free_wp(answer, &success);
    if (success) {
      printf("Delete watchpoint " FMT_WORD " success.\n", answer);
    }
    else {
      printf("Error!\n");
    }
  } else {
    printf("Error! please retry!\n");
  }
#else
  printf("Watch point not enabled\n");
#endif
  return 0;
}

static int cmd_w(char *args) {
#ifdef CONFIG_WATCH_POINT
  if (args == NULL) {
    printf("Error, please retry!\n");
    return 0;
  }
  char *expr = args;
  bool flag;
  WP *temp = new_wp(expr, &flag);
  if (flag) {
    printf("New watch point set success\nexpression = %s\n", temp->expression);
  }
  else {
    printf("Error! Watch point full\n");
  }
#else
  printf("Watch point not enabled\n");
#endif
  return 0;
}

static int cmd_x(char *args) {
  char *x_argv[2];
  if (args == NULL || split_args(args, x_argv, 2) < 2) {
    printf("Error, please retry!\n");
    return 0;
  }
  char *memory_len_str = x_argv[0];
  char *memory_addr_str = x_argv[1];
  bool len_flag = true;
  bool addr_flag = true;
  word_t memory_len = expr(memory_len_str, &len_flag);
  word_t memory_addr = expr(memory_addr_str, &addr_flag);
  if (len_flag && addr_flag) {
    for (int i = 0; i < LIST_NUM; i++) {
      printf("|    addr    |    data    |");
    }
    printf("\n");
    for (int i = 0; i < addr_cnt(memory_len); i++) {
      for (int j = 0; i + j * addr_cnt(memory_len) < memory_len; j++) {
        printf("| " FMT_WORD " : " FMT_WORD " |",
               (memory_addr + (i + j * addr_cnt(memory_len)) * 4),
               paddr_read(memory_addr + (i + j * addr_cnt(memory_len)) * 4, 4));
      }
      printf("\n");
    }
  } else {
    printf("Error! please retry!\n");
  }
  return 0;
}

static int cmd_info(char *args) {
  char *arg = get_arguments(args, 1);
  if (arg == NULL) {
    printf("Error, please retry!\n");
    return 0;
  }
  if (strcmp(arg, "r") == 0) {
    isa_reg_display();
  } else if (strcmp(arg, "w") == 0) {
  #ifdef CONFIG_WATCH_POINT
    check_wp();
  #else
    printf("Watch point not enabled\n");
  #endif
  }
  return 0;
}

static int cmd_trace(char *args) {
  char *arg = get_arguments(args, 1);
  if (arg == NULL) {
    printf("Error, please retry!\n");
    return 0;
  }
  if (strcmp(arg, "i") == 0) {
    IFDEF(CONFIG_ITRACE, display_iringbuf());
  } else if (strcmp(arg, "m") == 0) {
    IFDEF(CONFIG_MTRACE, display_mringbuf());
  } else if (strcmp(arg, "d") == 0) {
    IFDEF(CONFIG_DTRACE, display_dringbuf());
  } else if (strcmp(arg, "e") == 0) {
    IFDEF(CONFIG_ETRACE, display_etrace());
  } else if (strcmp(arg, "f") == 0) {
    IFDEF(CONFIG_FTRACE, display_call());
  } else if (strcmp(arg, "r") == 0) {
    IFDEF(CONFIG_RTRACE, display_rringbuf());
  }
  else {
    printf("Error, please retry!\n");
  }
  return 0;
}

static int cmd_si(char *args) {
  if (args == NULL) {
    cpu_exec(1);
    return 0;
  }
  bool flag = true;
  word_t steps = expr(args, &flag);
  if (flag) {
    cpu_exec(steps);
  } else {
    printf("Error, please retry!\n");
  }
  return 0;
}

IFDEF(CONFIG_DIFFTEST, extern bool enable_difftest);

static int cmd_attach(char *args) {
  IFDEF(CONFIG_DIFFTEST, isa_difftest_attach());
  printf("Enable difftest success\n");
  IFDEF(CONFIG_DIFFTEST, enable_difftest = true);
  return 0;
}

static int cmd_detach(char *args) {
  printf("Disable difftest success\n");
  IFDEF(CONFIG_DIFFTEST, enable_difftest = false);
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  IFDEF(CONFIG_WATCH_POINT, init_wp_pool());
}
