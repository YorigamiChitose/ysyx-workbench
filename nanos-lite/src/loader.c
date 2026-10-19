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

static uintptr_t loader(PCB *pcb, const char *filename) {
  (void)pcb;

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

void naive_uload(PCB *pcb, const char *filename) {
  uintptr_t entry = loader(pcb, filename);
  Log("Jump to entry = %p", (void *)entry);
  ((void(*)())entry) ();
}

