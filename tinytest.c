#include <dlfcn.h>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <memory.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef TINYTEST_CNF__NO_COLOR
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_RESET "\x1b[0m"
#else
#define ANSI_COLOR_RED ""
#define ANSI_COLOR_GREEN ""
#define ANSI_COLOR_RESET ""
#endif
typedef void (*tinytest_test_f)(void);

jmp_buf test_jumpback_buffer;
void test_signal_handler(int signum) { longjmp(test_jumpback_buffer, signum); }

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

  if (test_counter == 0)
    return 0;

  void *dl = dlopen(argv[1], RTLD_NOW | RTLD_GLOBAL);
  // TODO: do error handling on this one

  for (int i = 0; i < test_counter; i++)
    tests[i].test_f = (tinytest_test_f)dlsym(dl, tests[i].func_name);

  int signals[5] = {SIGILL, SIGTRAP, SIGABRT, SIGFPE, SIGSEGV};
  for (int i = 0; i < sizeof signals / sizeof(int); i++) {
    // TODO: replace this with sigaction
    signal(signals[i], test_signal_handler);
  }

  int tests_total = 0;
  int tests_passed = 0;
  for (int i = 0; i < test_counter; i++) {
    tests_total++;
    int signal_recved = 0;
    if ((signal_recved = setjmp(test_jumpback_buffer)) == 0) {
      printf("%.3d: began executing %s\n", i + 1, tests[i].test_name);
      tests[i].test_f();
      printf("     %s " ANSI_COLOR_GREEN "passed" ANSI_COLOR_RESET "...\n",
             tests[i].test_name);
      tests_passed++;
    } else {
      switch (signal_recved) {
      case SIGILL:
        printf("     caught illegal instruction\n");
        break;
      case SIGTRAP:
        printf("     assertion or breakpoint raised\n");
        break;
      case SIGABRT:
        printf("     abort signal was triggered\n");
        break;
      case SIGFPE:
        printf("     division by zero\n");
        break;
      case SIGSEGV:
        printf("     segmentation violation\n");
      }
      printf("     %s " ANSI_COLOR_RED "failed" ANSI_COLOR_RESET "...\n",
             tests[i].test_name);
    }
  }

  printf("Results: ");
  if (tests_passed == tests_total)
    printf(ANSI_COLOR_GREEN);
  else
    printf(ANSI_COLOR_RED);
  printf("%d/%d", tests_passed, tests_total);
  printf(ANSI_COLOR_RESET "\n");

  elf_end(elf);
  close(fd);

  return 0;
}