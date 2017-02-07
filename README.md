# RMC/Spock

RMC/Spock is the ROSE meta configuration system built on top of Spock package management.

The RMC layer is a few small shell scripts that allow a developer to
specify what features they need in a ROSE build tree, and the Spock
layer is responsible for finding a solution that satisfies the request
and configuring a shell environment to use that solution.

# Example

Here's an example session showing how to build ROSE with the latest
GNU Compiler Collection (GCC) version 4.x C/C++ compilers running in
their default mode (i.e., the gnu++03 and gnu98 languages), and Boost
1.62.x. The developer creates a text file named .rmc-main.cfg at the
top of the build tree, populates it with a few lines describing how to
configure ROSE, and then lets RMC figure out how to satisfy that. In
this example, RMC needs to download and install a number of packages.

    L1 $ mkdir rose-build-tree
    L1 $ cd rose-build-tree
    L1 $ cat >.rmc-main.cfg
    rmc_rosesrc /path/to/ROSE/source/tree
    rmc_compiler gnu-4-default
    rmc_boost 1.62
    ^D
    L1 $ rmc
    gnu-default-c++-4
    gnu-default-c-4
    boost-1.62
    cmake
    python-3
    spock-shell [INFO ]: solver took 10 steps
    spock-shell [INFO ]:   using spock=2.1.0@f3420764
    spock-shell [INFO ]:   using gnu-system-compilers=4.8.5@e4ee8ccf
    spock-shell [INFO ]:   using gnu-gnu++03=4.8.5@dc1397b2
    spock-shell [INFO ]:   using gnu-gnu89=4.8.5@7b5b12da
    spock-shell [ERROR]: missing zlib=1.2.11
    spock-shell [ERROR]: missing boost=1.62.0
    spock-shell [ERROR]: missing cmake=3.*
    spock-shell [INFO ]:   cmake available versions: 3.1.0 3.2.0 3.3.0 3.4.0 3.5.0 3.6.0 3.7.0
    spock-shell [ERROR]: missing python=3.6.0
    install zlib=1.2.11? [y] 
    spock-shell [INFO ]: downloading zlib=1.2.11
    spock-shell [INFO ]: building zlib=1.2.11@39c90bc5
    spock-shell [INFO ]:   using spock=2.1.0@f3420764
    spock-shell [INFO ]:   using gnu-system-compilers=4.8.5@e4ee8ccf
    spock-shell [INFO ]:   using gnu-gnu89=4.8.5@7b5b12da
    spock-shell [INFO ]:   using gnu-gnu++03=4.8.5@dc1397b2
    spock-shell [INFO ]: installing zlib=1.2.11@39c90bc5
    install boost=1.62.0? [y] 
    spock-shell [INFO ]: building boost=1.62.0@d0c7639c
    spock-shell [INFO ]:   using spock=2.1.0@f3420764
    spock-shell [INFO ]:   using gnu-system-compilers=4.8.5@e4ee8ccf
    spock-shell [INFO ]:   using gnu-gnu89=4.8.5@7b5b12da
    spock-shell [INFO ]:   using zlib=1.2.11@39c90bc5
    spock-shell [INFO ]:   using gnu-gnu++03=4.8.5@dc1397b2
    spock-shell [INFO ]: installing boost=1.62.0@d0c7639c
      3.1.0
      3.2.0
      3.3.0
      3.4.0
      3.5.0
      3.6.0
      3.7.0
    install cmake? [3.7.0] 
    spock-shell [INFO ]: downloading cmake=3.7.0
    spock-shell [INFO ]: building cmake=3.7.0@52efb605
    spock-shell [INFO ]:   using spock=2.1.0@f3420764
    spock-shell [INFO ]:   using gnu-system-compilers=4.8.5@e4ee8ccf
    spock-shell [INFO ]:   using gnu-gnu++03=4.8.5@dc1397b2
    spock-shell [INFO ]:   using gnu-gnu89=4.8.5@7b5b12da
    spock-shell [INFO ]:   using zlib=1.2.11@39c90bc5
    spock-shell [INFO ]:   using boost=1.62.0@d0c7639c
    spock-shell [INFO ]: installing cmake=3.7.0@52efb605
    install python=3.6.0? [y] 
    spock-shell [INFO ]: downloading python=3.6.0
    spock-shell [INFO ]: building python=3.6.0@52234340
    spock-shell [INFO ]:   using spock=2.1.0@f3420764
    spock-shell [INFO ]:   using gnu-system-compilers=4.8.5@e4ee8ccf
    spock-shell [INFO ]:   using gnu-gnu++03=4.8.5@dc1397b2
    spock-shell [INFO ]:   using gnu-gnu89=4.8.5@7b5b12da
    spock-shell [INFO ]:   using cmake=3.7.0@52efb605
    spock-shell [INFO ]:   using zlib=1.2.11@39c90bc5
    spock-shell [INFO ]:   using boost=1.62.0@d0c7639c
    spock-shell [INFO ]: installing python=3.6.0@52234340

    You are being placed into a new subshell whose environment is configured
    as you have requested.  You can further customize this environment by
    running additional spock-shell commands and dropping into deeper recursive
    subshells. When you're done, you can exit this shell to return to your
    previous environment. In Bash, the $SHLVL variable will indicate your
    nesting level.

    L2 $ rmc config --dry-run
    env CC='/home/matzke1/.spock/var/installed/7b5b12da/gnu89/bin/cc' CXX='/home/matzke1/.spock/var/installed/dc1397b2/gnu++03/bin/c++' FC='' /home/matzke1/GS-CAD/ROSE/matrix/source-repo/configure --with-CXXFLAGS='-fPIC -rdynamic -g' --with-CFLAGS='-fPIC -rdynamic -g' --disable-boost-version-check --disable-gcc-version-check --prefix='/tmp/matzke/junk/spock-test/installed' --with-ROSE_LONG_MAKE_CHECK_RULE=yes --with-boost='/home/matzke1/.spock/var/installed/d0c7639c/boost' --without-dlib --without-doxygen --without-dwarf --without-java --without-libreadline --without-magic --with-pch=no --with-python='/home/matzke1/.spock/var/installed/52234340/python/bin/python' --without-sqlite3 --without-wt --without-yaml --without-yices

    L2 $ rmc config
    [snip,snip]

    L2 $ rmc make -C src  # or use plain "make"
    [snip,snip]

    L2 $ exit
    L1 $ # Now back to original shell

# Installation

Most of the heavy lifting is done by the bootstrap.sh script. It needs
to download and build Spock's prerequesites, namely Boost, Yaml-cpp,
and Sawyer. Once it finishes, you no longer need the rmc-spock
directory since all run-time dependencies have been copied to the
installation prefix.  The default installation prefix is `$HOME/.spock`.

    BIG_FILESYSTEM=/mounts/nfs
    FAST_FILESYSTEM=/tmp

    git clone https://github.com/matzke1/rmc-spock
    (cd rmc-spock && ./scripts/bootstrap.sh --prefix=$BIG_FILESYSTEM/spock)
    rm -rf rmc-spock

    export SPOCK_ROOT="$BIG_FILESYSTEM/spock"
    export SPOCK_BLDDIR="$FAST_FILESYSTEM"
    export PATH="$SPOCK_ROOT/bin:$PATH"
