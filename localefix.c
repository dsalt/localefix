/*
  localefix

  Copyright (C) 2024 Darren Salt

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include <errno.h>
#include <dlfcn.h>
#include <stdio.h>

#ifdef DEBUG
#  include <unistd.h>
#  include <limits.h>
#else
// Not a debug build? Don't print anything at all
// (I know, kills validation of format strings...)
#  define fprintf(...)
#  define fputs(...)
#  define fputc(...)
#endif

#define LC_BAD  "_LC_BAD"
#define LC_GOOD "_LC_GOOD"
#define LC_ENC "_LC_ENCODING"

// Short, but longer than the longest in lc_vars[]
#define MAX_ENV_NAME_LENGTH 20
// Long enough for any locale name & encoding (& NUL terminator)
#define MAX_LOCALE_LENGTH 64

static const char lc_vars[][MAX_ENV_NAME_LENGTH] = {
  "LANGUAGE", // special: no locale
  "LANG",
  "LC_CTYPE",
  "LC_NUMERIC",
  "LC_TIME",
  "LC_COLLATE",
  "LC_MONETARY",
  "LC_MESSAGES",
  "LC_PAPER",
  "LC_NAME",
  "LC_ADDRESS",
  "LC_TELEPHONE",
  "LC_MEASUREMENT",
  "LC_IDENTIFICATION",
  "LC_ALL"
};
#define NUM_LC_VARS (*(&lc_vars + 1) - lc_vars)

// Target & replacement locale names (may contain encodings)
static const char *lc_bad = NULL, *lc_good = NULL;
// Replacement locale name (encoding stripped)
static char lc_language[MAX_ENV_NAME_LENGTH];
// Lengths of locale names (excluding encodings)
static int len_bad = -1, len_good = -1;

static int preserve_encoding;


static int (*real_setenv)(const char *, const char *, int) = NULL;
 int (*real_putenv)(char *) = NULL;

static inline int dotlen(const char *lc)
{
  const char *dot = strchr(lc, '.');
  return dot ? dot - lc : (int)strlen(lc);
}

static void __localefix_init(void)
{
  if (len_bad >= 0)
    return;

  lc_bad  = getenv(LC_BAD)  ?: "en_US";
  len_bad = dotlen(lc_bad); // should be 5

  lc_good = getenv(LC_GOOD) ?: "en_GB.*";
  int len = dotlen(lc_good); // should be 5

  // if _LC_GOOD ends in ".*", preserve the target encoding
  preserve_encoding = lc_good[len] && lc_good[len + 1] == '*';

  // make a copy of the locale (without encoding); note fixed-size buffer
  len = MIN(MAX_ENV_NAME_LENGTH - 1, len);
  strncpy(lc_language, lc_good, len);
  lc_language[len] = 0;

  len_good = len;
}

static int lookup_function(void **fn, const char *name)
{
  if (!*fn)
    *fn = dlsym(RTLD_NEXT, name);
  if (*fn)
    return 0; // done

  fprintf(stderr, "couldn't get %s!\n", name);
  errno = ENOSYS;
  return 1;
}
#define LOOKUP_FUNCTION(fn) lookup_function((void **)&real_##fn, #fn)

int setenv(const char *name, const char *value, int overwrite)
{
  __localefix_init();

  if (LOOKUP_FUNCTION(setenv))
    return 1;

  if (len_bad == 0 || len_good == 0)
    return real_setenv(name, value, overwrite);

  int i;
  for (i = 0; i < NUM_LC_VARS; ++i)
  {
    if (strcmp(name, lc_vars[i]))
      continue; // try next match

    if (!strncmp(value, lc_bad, len_bad)
        && (value[len_bad] == '.' || value[len_bad] == 0))
    {
      if (i && preserve_encoding)
      {
        char *dot = strchr(value, '.');
        // malloc & free?
        int len = len_good + (dot ? strlen(dot) : 0) + 1;
        if (len <= MAX_LOCALE_LENGTH)
        {
          char *replacement = alloca(len);
          value = replacement;
          sprintf(replacement, "%s%s", lc_language, dot ?: "");
        }
      }
      else
        value = i ? lc_good : lc_language;
      fputs(" [replaced]", stderr);
      break;
    }
  }
  fputc('\n', stderr);

  return real_setenv(name, value, overwrite);
}

int putenv(char *set)
{
  if (LOOKUP_FUNCTION(putenv))
    return 1;

  const char *value = strchr(set, '=');
  if (!value)
  {
    fputs(" [unset]\n", stderr);
    return real_putenv(set); // no '=' - unset the variable
  }
  if (value == set)
  {
    fputs(" [name?]\n", stderr);
    return errno = 0; // empty name, but '=' present - nothing to do
  }

  size_t namelen = ++value - set; // includes NUL/=; value now points past =
  if (namelen == 1 || namelen > MAX_ENV_NAME_LENGTH)
  {
    fputs(" [pass]\n", stderr);
    return real_putenv(set); // name's longer than any we handle - pass
  }

  // Maybe have something to replace at this point.
  // Split out the name and pass to our setenv().
  char name[namelen];
  name[namelen - 1] = 0;
  strncpy(name, set, namelen - 1);
  return setenv(name, value, 1);
}

static void __attribute__((constructor)) sanitise_environment(void)
{
  __localefix_init();

#ifdef DEBUG
  char exe[PATH_MAX] = "";
  char link[PATH_MAX] = "";
  int pid = getpid();
  snprintf(exe, PATH_MAX, "/proc/%d/exe", pid);
  ssize_t r = readlink(exe, link, PATH_MAX);
  if (r < 0)
    fprintf(stderr, "pid %d: executable? %d %s\n", pid, errno, strerror(errno));
  else
    fprintf(stderr, "pid %d: executable %*s\n", pid, (int)r, link);
#endif

  for (unsigned int i = 0; i < NUM_LC_VARS; ++i)
  {
    const char *current = getenv(lc_vars[i]);
    if (!current)
      continue; // not set? that's fine

    if (!strncmp(current, lc_bad, len_bad)
        && (current[len_bad] == '.' || current[len_bad] == 0))
      setenv(lc_vars[i], i ? lc_good : lc_language, 1);
  }
}
