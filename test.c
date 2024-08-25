#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#define ARRAY_SIZE(arr) (*(&arr + 1) - arr)

struct expect_t {
  const char *value;
  int ret, error;
};

struct pass_t {
  int value, ret, error;
};

static struct {
  struct pass_t r;
  int tests;
} tally = {};

#define QUOTE(v) (v ? "\"" : ""), (v ?: "null"), (v ? "\"" : "")

static void run_test(const char *set, const struct expect_t *expect)
{
  char *equals = strchr(set, '=');
  size_t namelen = equals ? (size_t)(equals - set) : strlen(set);
  char name[namelen];
  int ret, err;

  sprintf(name, "%.*s", (int)namelen, set);

  errno = 0;
  ret = putenv((char *)set);
  err = errno;

  const char *v = getenv(name);
  struct pass_t r = { v == expect->value || !strcmp(v ?: "", expect->value ?: ""), ret == expect->ret, err == expect->error };

  printf("Test: %-40s =>", set);
  printf(" %s: %s%s%s, %d, %s", (r.value && r.ret && r.error) ? "pass" : "FAIL", QUOTE(v), ret, strerrorname_np(err));
  if (r.value || r.ret || r.error)
    puts("");
  else
    printf(" (expected %s%s%s, %d, %s)\n", QUOTE(expect->value), expect->ret, strerrorname_np(expect->error));

  tally.r.value += r.value;
  tally.r.ret += r.ret;
  tally.r.error += r.error;
  tally.tests ++;
}

struct test_t {
  const char *set;
  struct expect_t r;
};

const struct test_t results_utf8[] = {
  // replace; enforced encoding UTF-8
  { "FOO=123",                  { "123", 0, 0 } },
  { "FOO=",                     { "", 0, 0 } },
  { "FOO",                      { NULL, 0, 0 } },
  { "FOO=en_US.UTF-8",          { "en_US.UTF-8", 0, 0 } },
  { "=123",                     { NULL, 0, 0 } },
  { "=en_US.UTF-8",             { NULL, 0, 0 } },
  { "",                         { NULL, 0, EINVAL } }, // arguably, this one captures a glibc implementation detail
  { "LC_TIME=en_US.UTF-8",      { "en_GB.UTF-8", 0, 0 } },
  { "LC_TIME=en_US",            { "en_GB.UTF-8", 0, 0 } },
  { "LC_TIME=en_US.ISO8859-15", { "en_GB.UTF-8", 0, 0 } },
  { "LC_TIME=en_IE.UTF-8",      { "en_IE.UTF-8", 0, 0 } },
  { "LANGUAGE=en_US",           { "en_GB", 0, 0 } },
  { "LC_TIME",                  { NULL, 0, 0 } },
  { "LC_TIME=",                 { "", 0, 0 } },
  { "IsTwentyLettersLong=foo",  { "foo", 0, 0 } },     // variable name is at least as long as MAX_ENV_NAME_LENGTH
  {}
}, results_C[] = {
  // replace; encoding dropped
  { "LC_TIME=en_US.UTF-8",      { "en_GB", 0, 0 } },
  { "LC_TIME=en_US",            { "en_GB", 0, 0 } },
  { "LC_TIME=en_US.ISO8859-15", { "en_GB", 0, 0 } },
  { "LANGUAGE=en_US",           { "en_GB", 0, 0 } },
  { "LANGUAGE=en_US.UTF-8",     { "en_GB", 0, 0 } }, // bad LANGUAGE value: contains an encoding
  {}
}, results_keep[] = {
  // replace; encoding retained
  { "LC_TIME=en_US.UTF-8",      { "en_GB.UTF-8", 0 ,0 } },
  { "LC_TIME=en_US",            { "en_GB", 0, 0 } },
  { "LC_TIME=en_US.ISO8859-15", { "en_GB.ISO8859-15", 0, 0 } },
  { "LANGUAGE=en_US",           { "en_GB", 0, 0 } },
  {}
}, results_null[] = {
  // no replacement
  { "LC_TIME=en_US.UTF-8",      { "en_US.UTF-8", 0, 0 } },
  { "LC_TIME=.UTF-8",           { ".UTF-8", 0, 0 } },
  { "FOO=en_US.UTF-8",          { "en_US.UTF-8", 0, 0 } },
  {}
};

static const struct testset_t {
  const char *bad, *good;
  const struct test_t *expect;
} testset[] = {
  { "en_US", "en_GB.UTF-8", results_utf8 },
  { "en_US", "en_GB",       results_C },
  { "en_US", "en_GB.*",     results_keep },
  { "",      "en_GB",       results_null },
  { "en_US", "",            results_null }
};

extern void __localefix_reread(void);

static void failure(int sig)
{
  printf("\nPasses: v%d r%d e%d of %d (incomplete: signal %d)\n", tally.r.value, tally.r.ret, tally.r.error, tally.tests, sig);
  exit(1);
}

int main(void)
{
  signal(SIGSEGV, failure);

  for (int set = 0; set < ARRAY_SIZE(testset); ++set)
  {
    printf("\n==> Set %d: from \"%s\" to \"%s\"\n", set, testset[set].bad, testset[set].good);

    setenv("_LC_GOOD", testset[set].good, 1);
    setenv("_LC_BAD", testset[set].bad, 1);
    __localefix_reread();

    for (const struct test_t *current = testset[set].expect; current->set; ++current)
      run_test(current->set, &current->r);
  }

  printf("\nPasses: v%d r%d e%d of %d\n", tally.r.value, tally.r.ret, tally.r.error, tally.tests);
  return !(tally.r.value == tally.tests && tally.r.ret == tally.tests && tally.r.error == tally.tests);
}
