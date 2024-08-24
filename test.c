#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

struct result_t {
  bool value, ret, error;
};

static struct {
  int value, ret, error, tests;
} tally = {};

static void test(const char *set, const char *Cv, int Cr, int Cerrno)
{
  char *equals = strchr(set, '=');
  size_t namelen = equals ? (size_t)(equals - set) : strlen(set);
  char name[namelen];
  sprintf(name, "%.*s", (int)namelen, set);

  errno = 0;
  int ret = putenv((char *)set);
  int err = errno;
  const char *v = getenv(name);
  static char yn[][4] = {"✗", "✓"};
  struct result_t r = { v == Cv || !strcmp(v?:"", Cv?:""), ret == Cr, err == Cerrno };
  printf ("Test: %-40s => %3d (%3d); pass: v%s r%s e%s; %c%s%c\n",
          set, ret, err, yn[r.value], yn[r.ret], yn[r.error], v?'"':0, v?:"NULL", v?'"':0);

  tally.value += r.value;
  tally.ret += r.ret;
  tally.error += r.error;
  tally.tests ++;
}

int main(void)
{
  test("LC_TIME=en_US.UTF-8", "en_GB.UTF-8", 0, 0);
  test("LC_TIME=en_IE.UTF-8", "en_IE.UTF-8", 0, 0);
  test("LANGUAGE=en_US", "en_GB", 0, 0);
  test("LC_TIME", NULL, 0, 0);
  test("LC_TIME=", "", 0, 0);
  test("FOO=123", "123", 0, 0);
  test("FOO=", "", 0, 0);
  test("FOO", NULL, 0, 0);
  test("FOO=en_US.UTF-8", "en_US.UTF-8", 0, 0);
  test("=123", NULL, 0, 0);
  test("=en_US.UTF-8", NULL, 0, 0);
  test("", NULL, 0, EINVAL); // yes, putenv (glibc) does this
  test("ALONGNAMETOOLONGFORUS=foo", "foo", 0, 0);

  printf("Passes: v%d r%d e%d of %d\n", tally.value, tally.ret, tally.error, tally.tests);
  return !(tally.value == tally.tests && tally.ret == tally.tests && tally.error == tally.tests);
}
