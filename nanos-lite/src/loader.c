#include <proc.h>
#include <elf.h>
#include <common.h>
#include <fs.h>

#ifdef __LP64__
# define Elf_Ehdr Elf64_Ehdr
# define Elf_Phdr Elf64_Phdr
#else
# define Elf_Ehdr Elf32_Ehdr
# define Elf_Phdr Elf32_Phdr
#endif

#define EXEC_MAX_ARGS 32
#define EXEC_ARG_BUF_SIZE 4096
#define EXEC_STACK_GAP (2 * 1024 * 1024)

typedef struct {
  int argc;
  int envc;
  char *argv[EXEC_MAX_ARGS + 1];
  char *envp[EXEC_MAX_ARGS + 1];
} ExecArgs;

static char exec_arg_buf[EXEC_ARG_BUF_SIZE];

static uintptr_t load_elf(const char *filename) {
  int fd = fs_open(filename, 0, 0);
  Elf_Ehdr ehdr;
  assert(fs_read(fd, &ehdr, sizeof(ehdr)) == sizeof(ehdr));
  assert(memcmp(ehdr.e_ident, ELFMAG, SELFMAG) == 0);
  assert(ehdr.e_phentsize == sizeof(Elf_Phdr));

  for (int i = 0; i < ehdr.e_phnum; i++) {
    Elf_Phdr phdr;
    size_t phoff = (size_t)ehdr.e_phoff + (size_t)i * ehdr.e_phentsize;
    assert(fs_lseek(fd, (intptr_t)phoff, SEEK_SET) == phoff);
    assert(fs_read(fd, &phdr, sizeof(phdr)) == sizeof(phdr));

    if (phdr.p_type != PT_LOAD) continue;
    assert(phdr.p_filesz <= phdr.p_memsz);
    assert(fs_lseek(fd, (intptr_t)phdr.p_offset, SEEK_SET) == phdr.p_offset);
    assert(fs_read(fd, (void *)(uintptr_t)phdr.p_vaddr, phdr.p_filesz) == phdr.p_filesz);

    if (phdr.p_memsz > phdr.p_filesz) {
      memset((void *)(uintptr_t)(phdr.p_vaddr + phdr.p_filesz), 0,
             phdr.p_memsz - phdr.p_filesz);
    }
  }

  fs_close(fd);
  return (uintptr_t)ehdr.e_entry;
}

static void copy_exec_args(const char *filename, char *const argv[], char *const envp[],
                           ExecArgs *args) {
  char *default_argv[] = {(char *)filename, NULL};
  char *const *src_argv = argv;
  if (src_argv == NULL || src_argv[0] == NULL) {
    src_argv = default_argv;
  }

  char *dst = exec_arg_buf;
  char *end = exec_arg_buf + sizeof(exec_arg_buf);

  args->argc = 0;
  while (src_argv[args->argc] != NULL && args->argc < EXEC_MAX_ARGS) {
    size_t len = strlen(src_argv[args->argc]) + 1;
    if (dst + len > end) panic("execve: arguments too large");
    memcpy(dst, src_argv[args->argc], len);
    args->argv[args->argc] = dst;
    dst += len;
    args->argc++;
  }
  args->argv[args->argc] = NULL;

  args->envc = 0;
  if (envp != NULL) {
    while (envp[args->envc] != NULL && args->envc < EXEC_MAX_ARGS) {
      size_t len = strlen(envp[args->envc]) + 1;
      if (dst + len > end) panic("execve: environment too large");
      memcpy(dst, envp[args->envc], len);
      args->envp[args->envc] = dst;
      dst += len;
      args->envc++;
    }
  }
  args->envp[args->envc] = NULL;
}

static uintptr_t build_user_stack(ExecArgs *args, uintptr_t top) {
  size_t strings = 0;
  for (int i = 0; i < args->argc; i++) {
    strings += strlen(args->argv[i]) + 1;
  }
  for (int i = 0; i < args->envc; i++) {
    strings += strlen(args->envp[i]) + 1;
  }

  size_t table = (size_t)(1 + args->argc + 1 + args->envc + 1) * sizeof(uintptr_t);
  uintptr_t sp = (top - strings - table) & ~(uintptr_t)15;
  uintptr_t *ptrs = (uintptr_t *)sp;
  char *str = (char *)(sp + table);

  ptrs[0] = args->argc;
  for (int i = 0; i < args->argc; i++) {
    size_t len = strlen(args->argv[i]) + 1;
    memcpy(str, args->argv[i], len);
    ptrs[1 + i] = (uintptr_t)str;
    str += len;
  }
  ptrs[1 + args->argc] = 0;

  for (int i = 0; i < args->envc; i++) {
    size_t len = strlen(args->envp[i]) + 1;
    memcpy(str, args->envp[i], len);
    ptrs[2 + args->argc + i] = (uintptr_t)str;
    str += len;
  }
  ptrs[2 + args->argc + args->envc] = 0;

  return sp;
}

__attribute__((noreturn))
static void enter_user(uintptr_t entry, uintptr_t sp) {
#ifdef __riscv
  asm volatile("mv sp, %0\n"
               "mv a0, %0\n"
               "jalr %1\n"
               : : "r"(sp), "r"(entry) : "memory");
#endif
  while (1) {
  }
}

__attribute__((noreturn))
static void uload(PCB *pcb, const char *filename, char *const argv[],
                  char *const envp[], uintptr_t stack_top) {
  (void)pcb;

  ExecArgs args;
  copy_exec_args(filename, argv, envp, &args);
  uintptr_t entry = load_elf(filename);
  uintptr_t sp = build_user_stack(&args, stack_top);

  Log("Jump to entry = %p, sp = %p", (void *)entry, (void *)sp);
  enter_user(entry, sp);
}

void naive_uload(PCB *pcb, const char *filename) {
  uload(pcb, filename, NULL, NULL, (uintptr_t)heap.end);
}

void execve_load(const char *filename, char *const argv[], char *const envp[]) {
  static int use_low_stack = 1;
  uintptr_t high = (uintptr_t)heap.end;
  uintptr_t low = high - EXEC_STACK_GAP;
  uintptr_t top = use_low_stack ? low : high;
  use_low_stack = !use_low_stack;
  uload(NULL, filename, argv, envp, top);
}
