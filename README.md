# localefix

This is intended for recalcitrant programs which limit you to a "blessed"
set of locales & languages which don't include yours, potentially causing
problems with the likes of date formats..

It consists of a wrapper script and one or more shared libraries (which will
be loaded via `LD_PRELOAD`).

It alters the initial values of `LANG`, `LANGUAGE` and the other locale
variables (names beginning with `LC_`) if needed.

It intercepts `setenv` and `putenv` to override considered-bad values.

The replacement locale also enforces encoding (at present; this may change).

## Rationale

Its reason for being is to work around Steam's game-specific configuration,
particularly the fact that if you want English, you get American English
instead.

Steam is a lot better with things like date formats these days, choosing
neutral (if not ideal) formats for them. However, things can break where it
offers game-specific choice of language: that's when it sets variables such
as `LC_MESSAGES`.

There's some variation here, but basically it boils down to some software
offering languages which Steam doesn't *but* not fully handling it: there
may be use of functions which get their locale data from an
incompletely-configured environment (external configuration), leading to
lookups being done for a language (or language variant) other than for which
the software is configured via its own means (internally).

I always select English (UK) where the option is offered but I may get
`mm/dd/yyyy` or, worse, a mix of date formats. This is why localefix's
defaults are what they are: it's what I need, and (so far as I know) it's
*the* common case of wrong date format.

## Usage

_[ENVIRONMENT]_ `localefix` _PROGRAM_ _[ARGS…]_

### Environment

* `_LC_BAD` - target locale without encoding
* `_LC_GOOD` - replacement locale (with encoding)

#### Defaults

```sh
_LC_BAD=en_US
_LC_GOOD=en_GB.UTF-8
```

Matching of `_LC_BAD` ignores encodings: the default will match `en_US`,
`en_US.UTF-8`, `en_US.ISO8859-15` etc.

The replacement locale enforces encoding (at present; this may change).

## Compilation

Requirements:

* `make` (usually GNU make)
* A C99-conformant C compiler (usually gcc or clang)

To compile, just run `make`.

If building on i386 or amd64, this will build two copies of the library, one
for each architecture. *You're probably going to want both.* You can
override this behaviour with `make SINGLEARCH=1`.

## Installation

Depends on where you're installing.

`make install`

`make install DESTDIR=foo`

The library will be installed in a directory named for the architecture
triplet, e.g. `/usr/local/lib/x86_64-linux-gnu`.
