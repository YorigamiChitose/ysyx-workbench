#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char *argv[], char *envp[]);
extern char **environ;
void call_main(uintptr_t *args) {
  char *empty[] = {NULL};
  int argc = 0;
  char **argv = empty;
  char **envp = empty;

#if defined(__riscv)
  if (args != NULL) {
    argc = (int)args[0];
    argv = (char **)(args + 1);
    envp = argv + argc + 1;
  }
#endif

  environ = envp;
  exit(main(argc, argv, envp));
  assert(0);
}
