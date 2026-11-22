# TinyMake

A small build tool for C projects. It reads a `build.txt`, builds a graph of the files to produce, and runs only the commands whose outputs are not up to date, in parallel if asked.

## Build

TinyMake builds itself. Bootstrap it once, then let it rebuild itself:

```sh
clang -Iinclude src/*.c -o tinymake && ./tinymake
```

Afterwards:

```sh
bin/tinymake        # one job at a time
bin/tinymake -j 8   # up to 8 jobs at the same time
```

## The build file

`build.txt` sits in the directory TinyMake is run from:

```
# Settings
cc = clang
ar = ar
cflags = -Wall -Wextra -O2
ldflags =
includes = include

# Targets: name, sources, then dependencies after '|'
library util: src/util.c
binary app: src/main.c src/app.c | util
```

- Objects go to `obj/`, binaries to `bin/<name>`, libraries to `lib/lib<name>.a`.
- `cc` is required; every other setting is optional.
- An output is rebuilt when it is missing, older than one of its sources or headers, or was built by a different command (tracked in `.tinymake_log`).

## Tests

```sh
bin/tinymake_test               # unit tests
test/e2e.sh bin/tinymake        # end-to-end tests
```

See [GLOSSARY.md](GLOSSARY.md) for the terms used in the code.
