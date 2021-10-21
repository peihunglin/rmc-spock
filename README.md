RMC/Spock
==========

The ROSE meta configuration system and Spock package management.

RMC/Spock is written by Robb Matzke and distributed at
[Github](https://github.com/matzke1/rmc-spock). It is meant as an
interim solution to provide strong consistency checking and software
development features while [Spack](https://spack.io) matures to
hopefully include similar features.

## Goal

RMC/Spock's primary goal is to make it easy for developers to compile
ROSE with any combination of software dependencies, including any
compiler, whether or not anyone has already installed those
dependencies on the system. It is intended to run on the development
platform of your choice, and currently supports RHEL 6 and 7; Debian 8
and 9; Ubuntu 16.04, 18.04, 18.10, and 19.04; Mint 18 and 19; and
Fedora 25 through 29. It may work on other Linux distributions as
well.

A secondary goal is to make it easier to test ROSE in automated
systems like Jenkins and the ROSE Matrix, by avoiding what has
historically been a long and frustrating process of installing
multiple configurations of software packages, and by mixing chosen
packages into an environment in a way that's consistent and
reproducible.

## Overview

"RMC" is an abbreviation for "ROSE Meta Configuration" since the
primary goal of this software is to configure ROSE's configuration
system. The top-level program in RMC/Spock is named "rmc", while
additional lower-level programs have names that start with
"spock-". "Spock" is a play of words on "Spack", which is a
Python-based HPC software installation system.

Although ROSE developers can use the "spock-*" commands, most will
instead use the "rmc" tool. The basic idea of "rmc" is:

1. Use "rmc init" to create and configure a new build directory
2. Use "rmc" to install all dependencies and start a build subshell
3. Use "rmc build" to run any ROSE pre-configure steps (modifies ROSE
   source tree)
4. Use "rmc config" to configure ROSE
5. Use "rmc make" to build ROSE
6. Use "rmc make install" to install ROSE
7. Don't forget to exit the "rmc" subshell when you're done

Step 1 is performed once per build directory; the other steps can be
done as often as you like in that build tree.  A ROSE developer can
modify the configuration of a ROSE build tree by editing the top-level
".rmc-main.cfg" file and re-running "rmc" (depending on your build
system, you may also need to do some cleaning before rebuilding).

## General per-user installation

RMC/Spock is intentionally easy to install into a ".spock"
subdirectory of the user's home. The entire procedure is:

    git clone https://github.com/matzke1/rmc-spock
	(cd rmc-spock && ./scripts/bootstrap.sh)
	rm -rf rmc-spock # optional
	export PATH="$HOME/.spock/bin:$PATH"

The bootstrap.sh script has command-line options to control which
versions of dependencies to use and whether it has permission to
download their source code from the Internet if it can't find the
source code locally.

RMC/Spock can be safely installed in ~/.spock even if the home
directory is NFS-mounted on different architectures. Each host will
have its own private areas under various architecture-dependent parts
of the ~/.spock tree.

TODO: Describe command-line options needed by various Linux
distributions. E.g., RHEL 6 has an old C++ compiler that can't handle
some of the more modern dependencies.

TODO: Describe how to install RMC/Spock on a machine with no Internet
connection.

## Installation of Intel compilers

Intel compilers are a little strange because they require
modifications to your shell environment before they can be used, and
these modifications are more involved than setting a couple
environment variables to easily determined values. This environmental
pollution also makes it difficult to use more than one version of
Intel compiler in the same shell session.

RMC/Spock tries to alleviate some of this pain by enclosing the Intel
compiler commands (found under /usr/apps/intel) in version-specific
wrapper scripts with names like icpc-16.0.3 and placing them in your
~/bin directory.

Tip: Now would be a good time to restart your login shell if you
polluted the environment by sourcing any Intel configuration
scripts. Running `exec bash` will probably not be sufficient! Also
note that "rmc" is really only well tested in Bash shells, although it
should work in others.

To generate these wrappers, run:

    ~/.spock/scripts/rose-install-intel-compilers

The Intel compilers should be usable at this point and can be tested,
but they also need to be added to the RMC/Spock database with:

    env PATH="$HOME/bin:$PATH" ~/.spock/scripts/spock-install-system-compilers

## Pre-fetching package source code (optional)

If RMC/Spock is used on a machine with no Internet connection, then
the source code for packages must be downloaded on a machine that does
have a connection to the Internet. Therefore, RMC/Spock must be
installed on both hosts.

On the host with an internet connection, the `spock-download` tool
can be used to obtain package source code. The arguments to
`spock-download` are package specifications. Some examples:

    spock-download boost=1.69.0 # download an exact version
	spock-download boost-1.69 # download versions with 1.69 as a prefix
	spock-download 'boost>=1.50' # download 1.50 or later
	spock-download boost # download all supported boost versions
	spock-download # download all supported packages
	
Beware that some package versions supported by RMC/Spock have been
removed from the distribution sites and may cause `spock-download` to
report errors. It is unfortunate that providers to this, some even
quite aggressively, since the ROSE project must test old software that
customers still use. Robb and Pei-Hung maintain repositories of old
package versions if you need them.

The `spock-download` tool places the downloads in the
~/.spock/var/downloads directory by default. They can be copied from
the Internet-connected host to the same directory on the unconnected
host.

## Removing packages

The `spock-rm` command recursively removes packages that are no longer
needed.

To remove all RMC/Spock packages, remove the ~/.spock/var/installed
directory.

To uninstall all or RMC/Spock including all packages that have been
installed and all software that has been downloaded, remove the
~/.spock directory.



ROSE work flow
===============

This chapter describes how to use RMC/Spock to develop ROSE. It is for
developers who intend to use the "rmc" command to make their work flow
easier.

## Creating a ROSE build tree

A ROSE build tree is a directory in which the ROSE object files,
libraries, and tools are created by compiling source files mostly from
the ROSE source tree.  In this documentation, the top directory of the
ROSE source tree is called "$ROSE_SOURCE" and the top directory of the
ROSE build tree is called "$ROSE_BUILD".

A ROSE build tree can be rooted under the ROSE source tree (usually
the top directory of that tree), or it can appear in any other
location in your file system. However, if `rmc` generates a ROSE build
tree configured for the Tup build system, then $ROSE_BUILD must be a
subdirectory of $ROSE_SOURCE.

The `rmc init` command initializes the build tree. It should be run
from the $ROSE_BUILD directory and its argument is $ROSE_SOURCE.

    mkdir $ROSE_BUILD
	cd $ROSE_BUILD
    rmc init $ROSE_SOURCE

The `rmc init` creates a ".rmc-main.cfg" file populated with defaults
according to its command-line switches and/or environment variables,
then (unless "--batch") invokes an editor on the file since
adjustments are frequently desired.

The ".rmc-main.cfg" file can be modified later by hand to alter the
configuration of the build environment.

## Entering a ROSE build environment

ROSE development happens inside the build tree in a special shell
environment. This environment is entered by running `rmc` from any
build tree directory, or running `rmc -C $DIR` to change to the
directory $DIR somewhere in the build tree.  The development
environment is a subshell configured to use selected packages.

If the ".rmc-main.cfg" specifies a package which is not currently
installed then `rmc` will install the package along with all
its dependencies that are also managed by RMC/Spock.

In order to change build environments but continue working in the same
build tree, one first exits the current development environment shell,
changes the .rmc-main.cfg file, and enters a new development
environment by running `rmc` again.

Hint: You might want to change your shell prompt when you're in a
build environment.

## Configuring ROSE

RMC/Spock supports three build systems: GNU Autotools, Tup, and
CMake. The "rmc_build" line in the .rmc-main.cfg determines which
build system is active in the current development environment.

All three build systems require a configuration step that detects what
software is available in the development environment and records that
information in various places to be used when compiling ROSE. The `rmc
config` command runs whatever configuration step is necessary, and can
be run from any directory in the build tree (or even outside the build
tree if you use its "-C" switch).

    rmc config

One normally runs `rmc config` from inside a development environment,
but if not, a new development environment will be created for the
duration of the configuration commands.

Hint: The "--dry-run" switch (or just "-n") prints the configuration
command instead of running it.  You can use this feature if you need
to make some minor adjustment to the configure command.

## Building ROSE

Building ROSE is the process of compiling the source code to create
the ROSE libraries, tools, and tests.  Depending on how the build tree
is configured, this involves running either "make" or "tup". In either
case, the `rmc make` command is a wrapper for the proper build
command.

Although use of `rmc make` is not required ("make" and "tup" can be
run directly), it does provide some useful features:

* `rmc make` turns on parallelism automatically for `make`
* `rmc make --verbose` causes the build process to show full commands
* `rmc make` uses conventional switch-then-argument order for tup commands
* `rmc make` turns off environment checking for tup
* `rmc make` ensures that the command is run in a development environment

Hint: If the build is run from inside Emacs, the `emacs` command
needn't be run from inside a development environment since the `rmc
make` will automatically provide one to the underlying `make` or `tup`
command.  In fact, one can run any command in a development
environment by giving its name, as in `rmc vi foo` to run `vi foo` in
a development environment. A select few commands ("make", "config", "init")
are overridden, in which case one needs to provide a full path to
avoid the override.



ROSE configuration options
=============================

As mentioned, the ".rmc-main.cfg" file at the top of your build tree
specifies how the build environment is created. The `rmc init` command
creates this file, populates it with defaults adjusted by
environment variables and command-line arguments, and offers you a
chance to make edits before entering the first build environment.
 
Believe it ore not, the ".rmc-main.cfg" file is simply a shell script,
although the only lines present initially all start with "rmc_". This
table describes what those "rmc_" lines mean, each of which take one
argument. The listed default values take effect if you remove the
"rmc_" command altogether. The value "ambivalent" means to let ROSE's
configuration system decide on a value.

* **rmc_rosesrc** specifies the path to the top of the ROSE source
  tree. There is no default value--this line is required.

* **rmc_build** specifies the type of build system to use, which should be
  one of the words "autoconf", "tup", or "cmake". The default is
  "autoconf".
  
* **rmc_parallelism** specifies the maximum amount of parallelism to use
  when building. The default, "system", means query the hardware and
  use that much. Otherwise the value should be a non-negative integer.
  
* **rmc_codegen** is the name of the code generator to use. Some systems
  can generate 32-bit or 64-bit executables. The valid arguments are
  "m32" and "default". The default is obviously "default".
  
* **rmc_optional_assertions** specifies whether optional assertions
  are "enabled" or "disabled". Required assertions are always enabled.
  The default is "enabled".

* **rmc_assertion_behavior** specifies what action ROSE takes when an
  assertion fails and the user hasn't overriden the action at
  runtime. The choices are "abort", "throw", "exit", or "ambivalent".
  The default is "ambivalent".

* **rmc_assertions** is present only for backward compatibility with
  portability testing. Its argument is the hyphenated value of the
  **rmc_optional_assertions** and **rmc_assertion_behavior**
  arguments. Either part is optional, in which case the hyphen is also
  omitted. Example "rmc_assertions enabled" is the same as
  "rmc_optional_assertions enabled", and "rmc_assertions
  enabled-abort" is the same as "rmc_optional_assertions enabled" and
  "rmc_assertion_behavior abort".
  
* **rmc_languages** specifies the languages to be supported by
  ROSE. Accepted values are the word "ambivalent" (the default),
  "none", or a comma-separated list of languages. The values used most
  often are "binaries", "c,c++", and "binaries,c,c++".
  
* **rmc_compiler** specifies the compilers to use. It's value is a
  hyphen-separated list of three parts: the name of the compiler
  vendor (gcc, llvm, or intel), the version number, and the language
  (c++03, c++11, c++14, c++17, gnu++03, gnu++11, gnu++14, or
  gnu++17). There is no default--this line is required.
  
* **rmc_debug** specifies whether the compiler should generate debug
  information when producing libraries, tools, and tests.  The values
  are "yes", "no", or the default "ambivalent".
  
* **rmc_optimize** specifies what optimization level should be used when
  compiling libraries, tools, and tests. The values are "yes", "no",
  or the default "ambivalent". The "ambivalent" amount is usually
  slightly less than "yes".
  
* **rmc_warnings** specifies whether the compiler should generate a
  liberal amount of warnings ("yes"), hardly any ("no"), or whatever
  amount ROSE would do by default ("ambivalent", the default).
  
* **rmc_binwalk** specifies which binwalk tool to use and must be a
  version, the default "none", or "ambivalent".
  
* **rmc_boost** specifies which Boost library to use and must be a
  version number. There is no default--this line is required.
  
* **rmc_cmake** specifies which CMake to use and must be a version
  number. The default is any version starting with "3". Even if CMake
  is not used as the ROSE build system, it's needed by ROSE
  dependencies.
  
* **rmc_cuda** specifies which Cuda to use. Currently, the only
  supported value is "none"
  
* **rmc_dlib** specifies which Dlib library to use and must be a
  version number, the default "none", or "ambivalent".
  
* **rmc_doxygen** specifies which Doxygen to use and must be
  a version number, the default "none", or "ambivalent".
  
* **rmc_dwarf** specifies which DWARF library to use and must be a
  version number, the default "none", or "ambivalent".
  
* **rmc_edg** specifies which EDG library to use (the C/C++ parser
  utilized by ROSE with C or C++ languages are enabled). The value
  must be a version number or the default "ambivalent". EDG is
  not required if ROSE is configured to lack C and C++ support.
  
* **rmc_elf** specifies which ELF library to use and must be a version
  number, the default "none", or "ambivalent". An ELF library is
  required only if libdwarf is used (see rmc_dwarf).
  
* **rmc_gcrypt** specifies which GNU cryptography library to use and
  must be a version number, the default "none", or "ambivalent".
  
* **rmc_java** specifies the Java development kit (JDK) to use and
  must be a version number optionally preceded by the vendor name and
  a hyphen, the default "none", or "ambivalent".
  
* **rmc_jq** specifies which Jq tool to use and must be a version
  number or the default "none", or "ambivalent". Jq is required when
  compiling the binary analysis tools distributed with ROSE, or when
  generating an LLVM compilation database when using the Tup build
  system.
  
* **rmc_lcov** specifies which lcov (code coverage) tool to use and
  must be a version number, the default "none", or "ambivalent".
  
* **rmc_libtool** specifies which libtool to use and must be a version
  number, the default "none", or "ambivalent". If the "rmc_build" mode
  is "autoconf" then a libtool is required, but may be provided by
  either RMC/Spock or the system.
  
* **rmc_magic** specifies which Magic library to use and must be a
  version number, the default "none", or "ambivalent".
  
* **rmc_pqxx** specifies which PostgreSQL library to use and must be a
  version number, the default "none", or "ambivalent".
  
* **rmc_python** specifies which Python to use and must be a version
  number, the default "none", or "ambivalent". Python is required by
  both the ROSE Python frontend and by ROSE's Python API.
  
* **rmc_qt** specifies which Qt GUI framework to use and must be a
  version number, the default "none", or "ambivalent".
  
* **rmc_raja** specifies which Raja library to use and must be a
  version number, the default "none", or "ambivalent".
  
* **rmc_readline** specifies which Readline library to use and must
  be a version number, the default "none", or "ambivalent".
  
* **rmc_spot** specifies which SPOT library to use and must be a
  version number, the default "none", or "ambivalent".
  
* **rmc_sqlite** specifies which SQLite3 library to use and must be a
  version number, the default "none", or "ambivalent".
  
* **rmc_tup** specifies which Tup tool to use and must be a version
  number, the word "none", or the default "ambivalent". Tup is
  required when "rmc_build" is "tup".
  
* **rmc_wt** specifies which Wt web toolkit to use and must be a
  version number, the default "none", or "ambivalent".
  
* **rmc_yaml** specifies which YAML-CC library to use and must be a
  version number, the default "none", or "ambivalent".
  
* **rmc_yices** specifies which Yices SMT solver to use and must be a
  version number, the default "none", or "ambivalent".
  
* **rmc_z3** specifies which Z3 SMT solver to use and must be a
  version number, the default "none", or "ambivalent".



RMC tips and tricks
=====================

Within a build environment, the shell variables $RG_SRC and $RG_BLD
refer to the top directories of the source and build trees,
respectively. $ROSE_SOURCE and $ROSE_BUILD are also defined.

The `rmc make foo bar baz` command operates differently from GNU Make
in that the targets are built one after another whereas `make -j foo
bar baz` builds them all at once.  This was done so one can say `rmc
make clean all`.

To run `make` in a project whose build tree is external to ROSE's
build tree, but which obtains the ROSE library and headers from ROSE's
build tree instead of using an installed version of ROSE, the "-C"
switch of both `rmc` and `make` can be used in conjunction: `rmc -C
$ROSE_BUILD make -C $(pwd)` first changes to the ROSE build tree to
find the .rmc-main.cfg file and create a build environment, and then
changes back to the original directory to run `make`.  This is useful
when compiling from inside an Emacs session that was started outside
any build environment.

To find out exactly how a package was compiled and installed by
RMC/Spock, look in the
~/.spock/var/installed/HOSTNAME/HASH/build-log.txt file, where
*HOSTNAME* is the machine's name and *HASH* is the package's
hash. Similar files document downloads in the ~/.spock/var/downloads
directory.

Make it clear that a build environment is active by incorporating
$SHLVL into your shell's prompt. Look
[here](https://github.com/matzke1/DeveloperScripts/tree/master/bash)
for some ideas.

  
Spock general commands
========================

This chapter describes by demonstration the lower-level "spock-*"
commands upon which the ROSE-specific "rmc" tool is built. It's
written in tutorial form and assumes that RMC/Spock has already been
installed. All spock commands show documentation if run with "--help".

First, let us list some things that are installed under RMC/Spock,
such as the C, C++, and Fortran compilers.  The "--help" documentation
for `spock-ls` describes the naming convention for compilers. Your
output may be slightly different than the output shown here.

List all compiler collections known to RMC/Spock on this system:

    $ spock-ls gnu-compilers
	gnu-compilers=6.5.0@ebd55a59 spock=2.2.0@9fbe326b
	gnu-compilers=7.1.0@8432c674 spock=2.2.0@9fbe326b
	gnu-compilers=7.2.0@76e1b6af spock=2.2.0@9fbe326b
	gnu-compilers=7.3.0@5e23b3d8 spock=2.2.0@9fbe326b
	gnu-compilers=8.1.0@9f083883 spock=2.2.0@9fbe326b
	gnu-compilers=8.2.0@b4fb548f spock=2.2.0@9fbe326b
	gnu-system-compilers=4.8.5@f776c5d5 spock=2.2.0@9fbe326b
	gnu-system-compilers=5.4.0@14d419e9 spock=2.2.0@9fbe326b
	$ spock-ls llvm-compilers
	llvm-compilers=4.0.1@9f78a38b spock=2.2.0@9fbe326b
	llvm-compilers=5.0.1@6bca5911 spock=2.2.0@9fbe326b
	llvm-compilers=5.0.2@64ba1b33 spock=2.2.0@9fbe326b
	llvm-compilers=6.0.0@13e7346f spock=2.2.0@9fbe326b
	llvm-compilers=6.0.1@569ab262 spock=2.2.0@9fbe326b
	llvm-compilers=7.0.0@c6bf7443 spock=2.2.0@9fbe326b
	llvm-system-compilers=5.0.0@e7b8a9d4 spock=2.2.0@9fbe326b
	$ spock-ls intel-compilers
	spock-ls [WARN ]: no package matching "intel-compilers"

This shows that three system-wide compiler collections are known to
RMC/Spock (two GNU collections and one LLVM collection), and 12
additional compiler collections have been installed by the user (six
GNU and six LLVM). There are no Intel compiler collections on this
machine.

Notice that the output lines are space-separated concrete package
specifications, each of which consists of a package name, followed by
a version number introduced with '=', and ending with a hash that
starts with "@".  The hash indicates that this is an installed
(concrete) package and uniquely identifies the installation.  The
subsequent specifications are the direct and indirect dependencies of
the first specification on the line. In this output, each compiler
collection depends on a particular RMC/Spock installation.

A compiler collection is just that: a collection of compilers. Using a
compiler collection doesn't necessarily mean that you're using any
particular compiler since each collection probably contains multiple C
compilers, multiple C++ compilers, and multiple Fortran compilers.

Terminology: "GNU" is treated as a vendor since it's an organization
that produces compilers. Likewise "LLVM" is treated as a vendor since
it's a project that creates compilers, although the actual compilers
of LLVM often fall into sub-projects with names like "Clang". The term
"GCC" (all upper case) is an abbreviation for "GNU Compiler
Collection" and thus is preceded by the definite article "the" when it
appears in this document. "GCC" should not be confused with "gcc",
which is the name of the C compiler executable distributed as
component of the GCC.

Let us list what C++ compilers are available:

    $ spock-ls c++-compiler
	gnu-c++03=4.8.5@48180e30 spock=2.2.0@9fbe326b gnu-system-compilers=4.8.5@f776c5d5 m32-generation=2.2.0@345c8d31
	gnu-c++03=4.8.5@ff71cf64 spock=2.2.0@9fbe326b gnu-system-compilers=4.8.5@f776c5d5 default-generation=2.2.0@e1b165d1
	gnu-c++03=5.4.0@45a45da2 spock=2.2.0@9fbe326b gnu-system-compilers=5.4.0@14d419e9 m32-generation=2.2.0@345c8d31
	gnu-c++03=5.4.0@61ed1df2 spock=2.2.0@9fbe326b gnu-system-compilers=5.4.0@14d419e9 m32-generation=2.2.0@345c8d31
	gnu-c++03=5.4.0@dc9e59e7 spock=2.2.0@9fbe326b gnu-system-compilers=5.4.0@14d419e9 default-generation=2.2.0@e1b165d1
	gnu-c++03=6.5.0@7c564164 spock=2.2.0@9fbe326b gnu-compilers=6.5.0@ebd55a59 default-generation=2.2.0@e1b165d1
	gnu-c++03=7.1.0@f4b9eb80 spock=2.2.0@9fbe326b gnu-compilers=7.1.0@8432c674 default-generation=2.2.0@e1b165d1
	gnu-c++03=7.2.0@6fbe2568 spock=2.2.0@9fbe326b gnu-compilers=7.2.0@76e1b6af default-generation=2.2.0@e1b165d1
	...
	llvm-gnu++17=6.0.0@81952225 spock=2.2.0@9fbe326b llvm-compilers=6.0.0@13e7346f m32-generation=2.2.0@345c8d31
	llvm-gnu++17=6.0.0@b4224e25 spock=2.2.0@9fbe326b llvm-compilers=6.0.0@13e7346f default-generation=2.2.0@e1b165d1
	llvm-gnu++17=6.0.1@ca22d916 spock=2.2.0@9fbe326b llvm-compilers=6.0.1@569ab262 m32-generation=2.2.0@345c8d31
	llvm-gnu++17=6.0.1@e28a2cc7 spock=2.2.0@9fbe326b llvm-compilers=6.0.1@569ab262 default-generation=2.2.0@e1b165d1
	llvm-gnu++17=7.0.0@0f0ed86a spock=2.2.0@9fbe326b llvm-compilers=7.0.0@c6bf7443 m32-generation=2.2.0@345c8d31
	llvm-gnu++17=7.0.0@dbc47af1 spock=2.2.0@9fbe326b llvm-compilers=7.0.0@c6bf7443 default-generation=2.2.0@e1b165d1

I've shown only the first and last few lines of the output since this
host has nearly 200 C++ compilers installed! Although the GCC's C++
compiler executable is named "g++", that executable masquerades as
multiple, binary-incompatible compilers and RMC/Spock treats it that
way. Therefore the compilers have package names consisting of the
vendor ("gnu" or "llvm" in the shown output) and the language ("c++03"
and "gnu++17" in the shown output). As documented in the `spock-ls
--help` output, there are other aliases as well.

Also, since compilers have more dependencies than compiler
collections, each line of output has more dependencies
specified. Besides the RMC/Spock installation, compilers also depend
on a compiler collection (the collection that provides the compiler
executable), and a code generator.  This host supports code generators
for 32- and 64-bit outputs.

The `spock-shell` command is responsible for two things: installing
packages and creating shell environments that use those packages.  Its
"--with" argument takes a list of package specifications which will be
employed in the shell environment. Let's say we want to use the
default C++ language compiler from the GCC 7.2.0. We know from the
above listing that the GCC version 7.2.0 is available, but we don't
know what language the default "g++" executable compiles. We can ask:

    $ spock-ls default-c++=7.2.0
	gnu-gnu++14=7.2.0@4172b106 spock=2.2.0@9fbe326b gnu-compilers=7.2.0@76e1b6af default-generation=2.2.0@e1b165d1

Aha, it's a GNU++14 compiler.

Let me pause momentatily to describe the version number part of a
package specification in a little more detail. So far, you've seen me
always say "=" in front of the version. The "=" is actually one of
many Spock version comparison operators and means "exactly". Another
version operator is "-" which means "starts with", as in "foo-7.2"
matches any foo package whose primary version number is 7 and whose
secondary version number is 2 and which may or may not have additional
parts in its version string. Other operators are ">" (later than), "<"
(earlier than), and the similar "<=" and ">=".  Now back to the
regularly scheduled program...

We asked `spock-ls` to tell us the name of the default c++ compiler
that has version number 7.2.0, but we didn't have to do that. The
`spock-shell` command will try to solve the package specification
constraints and choose a solution for us. For instance, we could ask
for the GCC version 7.2.* and its default C++ compiler and
`spock-shell` will try to obtain that combination:

    $ spock-shell --with gnu-compilers-7.2,default-c++
	spock-shell [INFO ]: solver took 4 steps
	spock-shell [INFO ]:   using spock=2.2.0@9fbe326b
	spock-shell [INFO ]:   using gnu-compilers=7.2.0@76e1b6af
	spock-shell [INFO ]:   using default-generation=2.2.0@e1b165d1
	spock-shell [INFO ]:   using gnu-gnu++14=7.2.0@4172b106
	L2 $
	
The output from `spock-shell` tells us that it found a solution and
what software packages are part of the solution. It then places us in
a new shell environment which I've indicated by changing the shell
prompt to "L2 $", meaning "level 2".

On this host, there are actually two solutions: one that generates
32-bit results and one that generates 64-bit (the default)
results. RMC/Spock gives preference to the default code generator. In
general, it also prefers packages that are already installed rather
than attempting to install something new.

RMC/Spock always installs C++ compilers to be named "c++" regardless
of what the vendor calls them.  If you get in the habit of invoking
the compiler as "c++" you don't need to remember (or detect in your
shell scripts) whether the compiler is named "g++", "clang++", "icpc",
etc.  Similarly for C compilers ("cc"), Fortran compilers ("fc"), D
compilers ("dc"), etc. In fact, RMC/Spock goes even one step further:
it removes other common names of compilers. Notice this in action:

    L2 $ c++ --version
    g++ (GCC) 7.2.0
    Copyright (C) 2017 Free Software Foundation, Inc.
    This is free software; see the source for copying conditions.  There is NO
    warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    L2 $ g++ --version
	g++ (GCC) 7.2.0
	Copyright (C) 2017 Free Software Foundation, Inc.
	This is free software; see the source for copying conditions.  There is NO
	warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	L2 $ clang++ --version
	/home/matzke/.spock/var/installed/wisconsin/4172b106/gnu++14/bin/clang++ is not a valid gnu gnu++14 compiler

	L2 $ icpc --version
	/home/matzke/.spock/var/installed/wisconsin/4172b106/gnu++14/bin/icpc is not a valid gnu gnu++14 compiler

RMC/Spock tries to protect us from doing other stupid things too. For
instance, it won't let us try to use a compiler that's incompatible
with what we're already using:

	L2 $ spock-shell --with gnu-c89-5.4
	spock-shell [INFO ]: solver took 1 steps
	spock-shell [ERROR]: gnu-compilers=5.4.0 conflicts with gnu-compilers=7.2.0@76e1b6af
	spock-shell [ERROR]: gnu-system-compilers=5.4.0@14d419e9 conflicts with gnu-compilers=7.2.0@76e1b6af
	spock-shell [ERROR]: m32-generation=2.2.0@345c8d31 conflicts with default-generation=2.2.0@e1b165d1
	spock-shell [ERROR]: no solutions found

The error tells us (among other things) that trying to use the C89
compiler from the GCC 5.4.0 would conflict with our current use of the
GCC 7.2.0. This is one of the things that Spack doesn't currently
support very well.

A quick aside: if you forget what packages you're using, run:

	L2 $ spock-using
	spock=2.2.0@9fbe326b
	gnu-compilers=7.2.0@76e1b6af
	default-generation=2.2.0@e1b165d1
	gnu-gnu++14=7.2.0@4172b106

However, `spock-shell` will let us use a different c89 compiler and
will even choose the correct one for us:

    L2 $ spock-shell --with c89-compiler
	spock-shell [INFO ]: solver took 2 steps
	spock-shell [INFO ]:   using spock=2.2.0@9fbe326b
	spock-shell [INFO ]:   using gnu-compilers=7.2.0@76e1b6af
	spock-shell [INFO ]:   using default-generation=2.2.0@e1b165d1
	spock-shell [INFO ]:   using gnu-c89=7.2.0@713fa79e
	spock-shell [INFO ]:   using gnu-gnu++14=7.2.0@4172b106
	L3 $
	
Notice that we're now at shell environment level 3. If we wanted to
drop the latest changes to the environment (just the c89 compiler) all
we need to do is `exit` from L3 back to L2.

The solver chose to use the C89 compiler from the GCC 7.2.0 since
that's the collection we were already using, and furthermore one that
generates 64-bit code (the default for this machine) since that's also
what we were already using. The `spock-ls` gives us all these details:

    L3 $ spock-ls @713fa79e
	gnu-c89=7.2.0@713fa79e spock=2.2.0@9fbe326b gnu-compilers=7.2.0@76e1b6af default-generation=2.2.0@e1b165d1
	
I said "@713fa78e" to demonstrate that since a hash uniquely
identifies a package, I can use just the hash when specifying a
package. I could have alternatively given the full name
"gnu-c89=7.2.0@713fa79e", but "gnu-c89" and "gnu-c89=7.2.0" match
multiple installations on this host and I would have had to look more
closely at the output.

One of the things that ROSE's configure script needs to do (or nearly
any software configuration for that matter) is detect what
dependencies have been provided. RMC/Spock tries to make this easy by
setting lots of environment variables. Here are a few (I filtered the
output to remove extraneous variables):

    L3 $ export
	declare -x ALL_INCDIRS="/home/matzke/.spock/var/installed/wisconsin/76e1b6af/gnu-compilers/include"
	declare -x ALL_LIBDIRS="/home/matzke/.spock/var/installed/wisconsin/76e1b6af/gnu-compilers/lib64:/home/matzke/.spock/var/installed/wisconsin/76e1b6af/gnu-compilers/lib"
	declare -x CXX_COMPILER="/home/matzke/.spock/var/installed/wisconsin/4172b106/gnu++14/bin/c++"
	declare -x CXX_LANGUAGE="gnu++14"
	declare -x CXX_ROOT="/home/matzke/.spock/var/installed/wisconsin/4172b106/gnu++14"
	declare -x CXX_VENDOR="gnu"
	declare -x CXX_VERSION="7.2.0"
	declare -x C_COMPILER="/home/matzke/.spock/var/installed/wisconsin/713fa79e/c89/bin/cc"
	declare -x C_LANGUAGE="c89"
	declare -x C_ROOT="/home/matzke/.spock/var/installed/wisconsin/713fa79e/c89"
	declare -x C_VENDOR="gnu"
	declare -x C_VERSION="7.2.0"
	declare -x GNU_COMPILERS_INCDIRS="/home/matzke/.spock/var/installed/wisconsin/76e1b6af/gnu-compilers/include"
	declare -x GNU_COMPILERS_LIBDIRS="/home/matzke/.spock/var/installed/wisconsin/76e1b6af/gnu-compilers/lib64:/home/matzke/.spock/var/installed/wisconsin/76e1b6af/gnu-compilers/lib"
	declare -x GNU_COMPILERS_ROOT="/home/matzke/.spock/var/installed/wisconsin/76e1b6af/gnu-compilers"
	declare -x GNU_COMPILERS_VERSION="7.2.0"
	declare -x SPACK_ROOT="/home/matzke/spack"
	declare -x SPACK_SHELL="bash"
	declare -x SPOCK_BINDIR="/home/matzke/.spock/bin"
	declare -x SPOCK_BLDDIR="/home/matzke/junk"
	declare -x SPOCK_EMPLOYED="9fbe326b:76e1b6af:e1b165d1:4172b106"
	declare -x SPOCK_HOSTNAME="wisconsin"
	declare -x SPOCK_OPTDIR="/home/matzke/.spock/var/installed/wisconsin"
	declare -x SPOCK_OS="Linux Mint 18.3"
	declare -x SPOCK_PKGDIR="/home/matzke/.spock/lib/packages"
	declare -x SPOCK_ROOT="/home/matzke/.spock"
	declare -x SPOCK_SCRIPTS="/home/matzke/.spock/scripts"
	declare -x SPOCK_SPEC="spock=2.2.0@9fbe326b"
	declare -x SPOCK_VARDIR="/home/matzke/.spock/var"
	declare -x SPOCK_VERSION="2.2.0"

Every RMC/Spock package, not just compilers, has certain environment
variables, among them variables to say where the package is installed
and which version it is. In addition, the compilers managed by
RMC/Spock have a few extra goodies not present consistently in normal
compilers. Chief among them is the ability to query the vendor,
language, and version:

	L3 $ c++ --spock-triplet
	gnu:gnu++14:7.2.0
	L3 $ cc --spock-triplet
	gnu:c89:7.2.0

Although having lots of compilers is fun, I haven't yet explained how
I installed so many compilers on this host. But before you complicate
your life with more compilers, let us install something else. You
should generally install software outside any RMC/Spock environment,
so type `exit` until you're back to your login shell (you improved
your shell prompts, right?)

Let us install Boost, which has moderately complex dependencies and
whose recipe file is well documented. RMC/Spock isn't as flexible as
Spack since it's meant more for ROSE development and less for general
installation of software on HPC systems. So while Spack can install
Boost in a whole host of configurations, RMC/Spock supports only the
two needed by ROSE: Boost with Python support ("boost-py"), and Boost
without Python support ("boost-nopy"). The virtual package "boost" can
refer to either variant just like the virtual package "c++-compiler" can
refer to lots of different C++ compilers: c++03-compiler,
c++11-compiler, gnu++14-compiler, etc.

Say we want Boost 1.63 compiled with a native (system-installed)
default (whatever language dialect that means) C++ compiler. We use
the "--install=yes" or "--install=ask" switch of `spock-shell` and
give our package constraints, including that of Boost:

	$ spock-shell --with gnu-system-compilers,default-c++,boost-1.63
	spock-shell [INFO ]: solver took 12 steps
	spock-shell [INFO ]:   using spock=2.2.0@9fbe326b
	spock-shell [INFO ]:   using gnu-system-compilers=5.4.0@14d419e9
	spock-shell [INFO ]:   using default-generation=2.2.0@e1b165d1
	spock-shell [INFO ]:   using gnu-gnu++03=5.4.0@bbcc828a
	spock-shell [INFO ]:   using gnu-gnu11=5.4.0@714eb6f2
	spock-shell [INFO ]:   using patchelf=0.9@53839b7a
	spock-shell [INFO ]:   using zlib=1.2.11@30854d01
	spock-shell [INFO ]:   using python=3.6.1@5c533fae
	spock-shell [ERROR]: missing boost=1.63.0
	$ 

Oops, we forgot to say we want missing packages installed before we
enter our new environment, thus we got an error and no new environment
was created. We try again:

	$ spock-shell --with gnu-system-compilers,default-c++,boost-1.63 --install=yes
	spock-shell [INFO ]: solver took 12 steps
	spock-shell [INFO ]:   using spock=2.2.0@9fbe326b
	spock-shell [INFO ]:   using gnu-system-compilers=5.4.0@14d419e9
	spock-shell [INFO ]:   using default-generation=2.2.0@e1b165d1
	spock-shell [INFO ]:   using gnu-gnu++03=5.4.0@bbcc828a
	spock-shell [INFO ]:   using gnu-gnu11=5.4.0@714eb6f2
	spock-shell [INFO ]:   using patchelf=0.9@53839b7a
	spock-shell [INFO ]:   using zlib=1.2.11@30854d01
	spock-shell [INFO ]:   using python=3.6.1@5c533fae
	spock-shell [ERROR]: missing boost=1.63.0
	spock-shell [INFO ]: downloading boost=1.63.0
	spock-shell [INFO ]: building boost=1.63.0@f48f3ede
	spock-shell [INFO ]:   using spock=2.2.0@9fbe326b
	spock-shell [INFO ]:   using gnu-system-compilers=5.4.0@14d419e9
	spock-shell [INFO ]:   using default-generation=2.2.0@e1b165d1
	spock-shell [INFO ]:   using gnu-gnu++03=5.4.0@bbcc828a
	spock-shell [INFO ]:   using gnu-gnu11=5.4.0@714eb6f2
	spock-shell [INFO ]:   using patchelf=0.9@53839b7a
	spock-shell [INFO ]:   using zlib=1.2.11@30854d01
	spock-shell [INFO ]:   using python=3.6.1@5c533fae
	spock-shell [INFO ]: installing boost=1.63.0@f48f3ede

	L2 $ spock-using boost # what Boost are we using?
	boost=1.63.0@f48f3ede
	
	L3 $ spock-ls boost=1.63.0@f48f3ede # what are its dependencies?
	boost=1.63.0@f48f3ede gnu-gnu++03=5.4.0@bbcc828a python=3.6.1@5c533fae spock=2.2.0@9fbe326b zlib=1.2.11@30854d01

	L2 $ export |grep BOOST # some environment variables
	declare -x BOOST_INCDIRS="/home/matzke/.spock/var/installed/wisconsin/f48f3ede/boost/include"
	declare -x BOOST_LIBDIRS="/home/matzke/.spock/var/installed/wisconsin/f48f3ede/boost/lib"
	declare -x BOOST_ROOT="/home/matzke/.spock/var/installed/wisconsin/f48f3ede/boost"
	declare -x BOOST_VERSION="1.63.0"

Notice that `spock-shell` automatically brought in a GNU11 C compiler,
that the default C++ compiler for the GCC 5.4.0 is a GNU++03 compiler,
and that we also need the patchelf, zlib, and python packages which
happen to be already installed on this host since I've previously
installed lots of other Boost configurations with these
compilers. Normally `spock-shell` will also install the dependencies
if necessary.

Caveat: Although RMC/Spock distinguishes between run-time dependencies
and build-time dependencies, it doesn't do a super good job of
handling them. If during the installation you get strange errors from
the solver saying it couldn't find a solution, or you get failed
assertions from `spock-shell`, try figuring out the package's
dependencies by looking at its recipe in ~/.spock/lib/packages and
then use `spock-shell` to build the dependencies before going back and
building the main package. Also double check that you're not already
running in a `spock-shell` environment by checking the $SHLVL variable
(you did improve your shell prompt finally, right?)

Well, that's pretty much all you need to know about installing and
using packages in RMC/Spock.  But you might want to know how to safely
remove a package those packages that depend on it. That's where the
`spock-rm` tool comes in. The `spock-rm` has a few modes, but if you
want just delete a package and all it's dependencies, just give the
package specification as an argument, probably including the hash
since otherwise it would remove everything that matches. You can use
"--dry-run" to see what would be removed:

	$ spock-rm --dry-run zlib-1.2.11@30854d01
	z3=4.8.1@529edc3b
	binwalk=2.1.1@552449f5
	python=3.6.1@f4e9c692
	python=3.6.1@5c533fae
	liblcms=2.8@5ba732b7
	libtiff=4.0.9@98a538d0
	libpng=1.6.34@5e6b78cd
	libgraphicsmagick=1.3.25@ba761457
	yamlcpp=0.5.3@6492deda
	wt=3.3.7@00e3991f
	boost-nopy=1.62.0@47e57b4c
	boost-nopy=1.62.0@a6268619
	boost-nopy=1.68.0@43281906
	boost-nopy=1.69.0@985bafb0
	boost=1.62.0@ce702bb5
	boost=1.63.0@f48f3ede
	zlib=1.2.11@30854d01

Whoa, that's a bunch more than I expected. Let's not remove that. How
about removing the boost we just installed:

    $ spock-rm --dry-run boost=1.63.0@f48f3ede
    boost=1.63.0@f48f3ede

Aha, better--just one package.  If you're tight on disk space, you can
also periodically run with the "--stale D" flag that removes packages
that haven't been used in at least *D* days. Beware of removing
individual compilers since the only way to reinstall them is to
install the compiler collection to which they belong.

Which brings us back around to an earlier question: How does one
install a compiler? Recall that a compiler usually belongs to a
compiler collection, and compiler collections have names like
"gnu-compilers" and "llvm-compilers" (you can't install
"intel-compilers" automatically with RMC/Spock because you need to pay
for them). Therefore, instead of installing a particular compiler,
install the collection which will also install all its compilers:

    $ spock-shell --with gnu-compilers-8.1.0 --install=yes true
	
Hint: The "true" at the end causes `spock-shell` to execute `true` in
the build environment instead of starting a new interactive shell. You
could actually put any command and arguments you want there as
described earlier for the "rmc" command.
