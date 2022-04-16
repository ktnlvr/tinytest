#include <dlfcn.h>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef void (*tinytest_test_f)(void);

static const char separator[] = "-------------------------\n";

typedef struct tinytest_test {
  const char *test_name;
  const char *func_name;
  tinytest_test_f test_f;
} tinytest_test;

int main(int argc, char **argv) {
  size_t test_counter = 0;
  size_t test_capacity = 16;
  tinytest_test *tests =
      (tinytest_test *)malloc(sizeof(tinytest_test) * test_capacity);

  elf_version(EV_CURRENT);

  int fd = open(argv[1], O_RDONLY);
  Elf *elf = elf_begin(fd, ELF_C_READ, NULL);
  Elf_Scn *section = NULL;
  GElf_Shdr shdr;

  while ((section = elf_nextscn(elf, section)) != NULL) {
    gelf_getshdr(section, &shdr);
    if (shdr.sh_type == SHT_SYMTAB)
      // Symbol table located
      break;
  }

  printf(separator);

  Elf_Data *data;
  data = elf_getdata(section, NULL);
  int count = shdr.sh_size / shdr.sh_entsize;

  int ii;
  for (ii = 0; ii < count; ++ii) {
    GElf_Sym symbol;
    gelf_getsym(data, ii, &symbol);

    const char *prefix = "tinytest_test__";
    int prefix_len = strlen(prefix);
    const char *funcname = elf_strptr(elf, shdr.sh_link, symbol.st_name);
    if (strlen(funcname) >= strlen(prefix))
      if (memcmp(funcname, prefix, strlen(prefix)) == 0) {
        if (test_counter >= test_capacity) {
          test_capacity *= 2;
          tests =
              (tinytest_test *)realloc(tests, sizeof(tests) * test_capacity);
        }

        int test_name_len = strlen(funcname + prefix_len) - 2;

        char *test_name = (char *)malloc(sizeof(char) * test_name_len);
        strncpy(test_name, funcname + prefix_len, test_name_len);
        tests[test_counter].test_name = test_name;

        char *func_name = (char *)malloc(sizeof(char) * strlen(funcname));
        strcpy(func_name, funcname);
        tests[test_counter].func_name = func_name;

        ++test_counter;
      }
  }

  printf("Gathered %d tests:\n", test_counter);
  for (int i = 0; i < test_counter; i++)
    printf("%.3d: %s\n", i + 1, tests[i].test_name);

  printf(separator);

  if (test_counter == 0)
    return 0;

  printf("Loading...\n");
  void *dl = dlopen(argv[1], RTLD_NOW | RTLD_GLOBAL);
  // TODO: do error handling on this one

  for (int i = 0; i < test_counter; i++)
    tests[i].test_f = (tinytest_test_f)dlsym(dl, tests[i].func_name);

  printf(separator);

  printf("Executing....\n");
  for (int i = 0; i < test_counter; i++) {
    printf("%.3d: began executing %s\n", i + 1, tests[i].test_name);
    tests[i].test_f();
    printf("     %s passed...\n", tests[i].test_name);
  }

  elf_end(elf);
  close(fd);

  return 0;
}