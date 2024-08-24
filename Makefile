#! /usr/bin/make

CFLAGS   = -O2 -g
OPTFLAGS = -Wall -Wextra -Wformat=2
LDFLAGS  = -ldl
override LIBFLAGS = -shared -fPIC

# must not contain '%' or '"' else nasal daemons may ensue
PREFIX = /usr/local
# assuming BINDIR=$(PREFIX)/bin
# assuming LIBDIR=$(PREFIX)/lib

# Installs, by default, into /usr/local/lib/FOO
# where FOO is something like x86_64-linux-gnu

ifneq ($(DEBUG),)
  CFLAGS = -Og -g -DDEBUG
endif

# Build only on host arch for host arch (for now)
# Compiler must support -dumpmachine and output something like arch[-pc]-kernel-type
ARCH = $(shell $(CC) -dumpmachine | sed -re 's%[./]%_%g; s/-pc-/-/')

DUPLET := $(word 2,$(subst -, ,$(ARCH)))-$(word 3,$(subst -, ,$(ARCH)))

# Will need fixup if your arch names aren't "x86_64" and "i386"
ifeq ($(subst x86_64,---,$(subst i386,---,$(ARCH)$(SINGLEARCH))),----$(DUPLET))
  $(info Building for x86_64 & i386)
  MULTIARCH := yes
  LIBRARIES := lib/i386-$(DUPLET)/localefix.so lib/x86_64-$(DUPLET)/localefix.so
else
  $(info Building for $(firstword $(subst -, ,$(ARCH))))
  MULTIARCH :=
  LIBRARIES := lib/$(ARCH)/localefix.so
endif

all: $(LIBRARIES) bin/localefix

ifeq ($(MULTIARCH),yes)
lib/i386-$(DUPLET)/localefix.so: localefix.c
	mkdir -p $(dir $@)
	$(CC) -m32 $(CFLAGS) $(OPTFLAGS) $(LIBFLAGS) $(LDFLAGS) $< -o $@

lib/x86_64-$(DUPLET)/localefix.so: localefix.c
	mkdir -p $(dir $@)
	$(CC) -m64 $(CFLAGS) $(OPTFLAGS) $(LIBFLAGS) $(LDFLAGS) $< -o $@

else
lib/$(ARCH)/localefix.so: localefix.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OPTFLAGS) $(LIBFLAGS) $(LDFLAGS) $< -o $@

endif

bin/localefix: localefix.in
	mkdir -p bin
	sed -e 's%{{PREFIX}}%'"$(PREFIX)"'%' >$@ <$<

clean:
	rm -f $(LIBRARIES) bin/localefix test
	rmdir -p --ignore-fail-on-non-empty $(dir $(LIBRARIES)) bin || :

STR := $

install: all
	install -d $(addprefix $(DESTDIR)$(PREFIX)/,$(dir $(LIBRARIES) bin/))
	for i in $(LIBRARIES); do \
	  install -m755 -s $(STR)i $(DESTDIR)$(PREFIX)/$(STR)(dirname $(STR)i); \
	done
	install -m755 bin/localefix $(DESTDIR)$(PREFIX)/bin/

uninstall:
	rm $(addprefix $(DESTDIR)$(PREFIX)/,$(LIBRARIES) bin/localefix)

# Testing

# Defaults specified in case of environment
run-tests: test lib/$(ARCH)/localefix.so
	LD_PRELOAD=lib/$(ARCH)/localefix.so _LC_BAD=en_US _LC_GOOD=en_GB.UTF-8 ./test

# Test binary. Returns non-zero on failure
test: test.c
	$(CC) $(CFLAGS) $(OPTFLAGS) $< -o $@


.DELETE_ON_ERROR:
