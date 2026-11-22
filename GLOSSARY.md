# TinyMake

A small build tool for C projects. It reads a build file, turns its targets into a graph of files, and runs only the commands whose outputs are not up to date.

## Build file

**Build file**:
The `build.txt` file in the current directory that declares the settings and targets of a project.
_Avoid_: config, makefile

**Setting**:
A `key = values` line of the build file. The keys are `cc` (compiler, required), `ar` (archiver, defaults to `ar`), `cflags` (code: `compile_flags`), `ldflags` (code: `link_flags`, only used when linking binaries) and `includes` (code: `include_dirs`).
_Avoid_: key, option, variable

**Target**:
A `binary` or `library` line of the build file: a name, its sources, and optionally its dependencies after a `|`. Binaries and libraries share one namespace.
_Avoid_: rule, product

**Binary**:
A target linked into an executable at `bin/<name>`.
_Avoid_: executable, program

**Library**:
A target archived into a static library at `lib/lib<name>.a`. Libraries are static only.
_Avoid_: archive

**Dependency**:
A library that a target names after `|`, and that is linked into every binary depending on it, directly or not. Dependencies must not form a cycle.
_Avoid_: deps, linked library; never use it for graph edges or depfile entries

**Source**:
A file listed by a target, compiled into one object. Sources are never built by TinyMake.
_Avoid_: input file

## Graph

**Node**:
One file of the build and how to make it, identified by its output. A source is a node with no command.
_Avoid_: file, task, step

**Output**:
The path of the file a node stands for.
_Avoid_: product, artifact

**Object**:
The node that compiles one source, at `obj/<source path without extension>.o`, with its depfile beside it.
_Avoid_: object file, `.o`

**Input**:
A node that another node is built from, as an edge of the graph. Discovered inputs are not inputs.
_Avoid_: dependency, prerequisite

**Dependent**:
A node built from a given node; the reverse of input.
_Avoid_: dependency, consumer

**Command**:
The arguments a node runs to make its output, and their joined form as shown and recorded in the command log.
_Avoid_: rule, recipe, job

**Depfile**:
A make-style file written by the compiler (`-MMD`) next to an object, listing the files that object was compiled from.
_Avoid_: dependency file, `.d` file

**Discovered input**:
A file listed in a node's depfile, in practice a header. It counts when checking whether the node is up to date, but is not a node.
_Avoid_: prerequisite, header dependency, implicit dependency

## Running a build

**Build**:
One run of TinyMake over the build file.

**Up to date**:
A node whose output exists, is newer than all of its inputs and discovered inputs, and whose command matches the one in the command log. An up to date node is skipped; its negation is "not up to date".
_Avoid_: fresh, stale, dirty, clean

**State directory**:
The `.tinymake/` directory holding everything TinyMake keeps between builds, such as the command log. It is created when the command log is saved.
_Avoid_: cache directory; never shorten it to "state" (see **Node status**)

**Command log**:
The `.tinymake/commands` file recording, for each output, the last command that built it successfully, so that an output is rebuilt when its command changes.
_Avoid_: log, cmdlog (except in code)

**Node status**:
Where a node is in a build: waiting (some inputs not done), ready, running, done (built, skipped, or a source), or failed. A failure fails all dependents transitively, and no new job starts after it.
_Avoid_: state

**Job**:
One command running as a process, with its output captured. `-j <n>` sets how many jobs may run at the same time.
_Avoid_: task, worker, process
