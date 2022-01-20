# Shell functions for installing compilers

source "$SPOCK_SCRIPTS/impl/basic-support.sh" || exit 1
source "$SPOCK_SCRIPTS/impl/spock-realpath.sh" || exit 1

# Generate compiler switches for code generation specifications. Code generation specifications are one or more words
# each prefixed with a "-" and no intervening white space. For instance, "-m32-debug" means 32-bit mode with debugging.
spock-compiler-code-generation-switches() {
    local code_generation="$1"
    local word=
    for word in $(echo "$code_generation" |tr '-' ' '); do
        case "$word" in
            m32) echo -n " -m32" ;;
            *) die "unknown compiler code generation specification '$word'" ;;
        esac
    done
    echo
}

# Generate a suitable shell variable to describe a base language
spock-compiler-base-language-var() {
    local base_language="$1"
    case "$base_language" in
        c++) echo "CXX" ;;
        c) echo "C" ;;
        fortran) echo "FORTRAN" ;;
        java) echo "JAVA" ;;
	cuda) echo "CUDA" ;;
        *) die "unknown base language: $base_language" ;;
    esac
}

# Enable or disable a compiler command by linking it to either spock-compiler or not-a-compiler. Compilers are enabled
# when $lhs = $rhs and otherwise disabled.
spock-compiler-onoff() {
    local lhs="$1" rhs="$2" exe="$3"
    if [ "$lhs" = "$rhs" ]; then
        ln -sf spock-compiler "$exe"
    else
        ln -sf not-a-compiler "$exe"
    fi
}

# Create the package files for a specific compiler. Echos the package spec that was installed.  This function does not
# check if the compiler is already installed -- it just installs a new one.
spock-compiler-install() {
    local compiler_quad="$1" collection_spec="$2" code_generation="$3" executable="$4"; shift 4 # compiler args

    local compiler_vendor=$(echo "$compiler_quad" |cut -d: -f1)
    local compiler_baselang=$(echo "$compiler_quad" |cut -d: -f2 -s)
    local compiler_lang=$(echo "$compiler_quad" |cut -d: -f3 -s)
    local compiler_version=$(echo "$compiler_quad" |cut -d: -f4 -s)

    [ -n "$compiler_quad"     ] || die "no compiler quad"
    [ -n "$compiler_vendor"   ] || die "no compiler vendor in quad $compiler_quad"
    [ -n "$compiler_baselang" ] || die "no compiler baselang in quad $compiler_quad"
    [ -n "$compiler_lang"     ] || die "no compiler lang in quad $compiler_quad"
    [ -n "$compiler_version"  ] || die "no compiler version in quad $compiler_quad"
    [ -n "$collection_spec"   ] || die "no compiler collection specification"
    [ -n "$executable"        ] || die "no compiler executable specified"

    while true; do
        local package_hash=$(echo "$RANDOM" |sha1sum |cut -c1-8)
        local package_yaml="$SPOCK_OPTDIR/$package_hash.yaml"
        local package_root="$SPOCK_OPTDIR/$package_hash/$compiler_lang"
        local package_bindir="$package_root/bin"
        local package_spec="${compiler_vendor}-${compiler_lang}=${compiler_version}@${package_hash}"
        [ ! -e "$package_yaml" -a ! -e "$package_root" ] && break
    done
    mkdir -p "$package_bindir" || exit 1

    # Compilers sometimes need to add some compiler-specific libraries to the shared object search path in order for the
    # programs compile with that compiler to be able to run. Intel expects users to choose one Intel compiler by
    # sourcing a setup.sh file into their environment, but we want to delay that until spock chooses the
    # compiler. Really, all we need is just the LD_RUN_PATH, so the Intel compiler wrappers support a switch that
    # returns that info without running the compiler. GNU compilers already have a --print-search-dirs switch that
    # does something similar (I'm not sure it's needed though [matzke, 2017-03-15].
    compiler_libdirs=
    case "$compiler_vendor" in
        #gnu|llvm)
        #    compiler_libdirs="$($executable --print-search-dirs 2>/dev/null | sed -n '/^libraries:/ s/^libraries: *=\? *//p')"
        #    ;;
        intel)
            compiler_libdirs="$($executable --spock-so-paths 2>/dev/null)"
            ;;
    esac

    # Create the compiler executables in the package's "bin" directory. This directory also needs the YAML configuration
    # file so the spock-compiler wrapper knows how to run the real compiler.
    (
        set -e
        cd "$package_bindir"
        [ "$SPOCK_HOSTNAME" = "" ] && SPOCK_HOSTNAME="$(hostname --short)"
        cp "$SPOCK_BINDIR/$SPOCK_HOSTNAME/spock-compiler" "."

        (
            echo "#!/bin/bash"
            echo "echo \"\$0 is not a valid $compiler_vendor $compiler_lang compiler\" >&2"
            echo "exit 1"
        ) >not-a-compiler
        chmod 755 not-a-compiler

        local cg_switches=$(spock-compiler-code-generation-switches "$code_generation")
        "$SPOCK_SCRIPTS/impl/detect-compiler-characteristics" \
            --yaml --baselang="$compiler_baselang" "$executable" $cg_switches "$@" >"$package_bindir/compiler.yaml"

        # Enable and disable programs that run compilers
        case "$compiler_baselang" in
            c++)
                spock-compiler-onoff "$compiler_vendor" gnu   g++
                spock-compiler-onoff "$compiler_vendor" intel icpc
                spock-compiler-onoff "$compiler_vendor" llvm  clang++

                spock-compiler-onoff x                  x     c++
                spock-compiler-onoff x                  x     cxx
                ;;

            c)
                spock-compiler-onoff "$compiler_vendor" gnu   gcc
                spock-compiler-onoff "$compiler_vendor" intel icc
                spock-compiler-onoff "$compiler_vendor" llvm  clang

                spock-compiler-onoff x                  x     cc
                ;;

            fortran)
                spock-compiler-onoff "$compiler_vendor" gnu   gfortran
                spock-compiler-onoff "$compiler_vendor" intel ifort

                spock-compiler-onoff x                  x     fc
                ;;

	    cuda)
		spock-compiler-onoff "$compiler_vendor" nvidia nvcc
        esac
    ) || exit 1

    local compiler_cmd=
    case "$compiler_baselang" in
        c++)     compiler_cmd=c++  ;;
        c)       compiler_cmd=cc   ;;
        fortran) compiler_cmd=fc   ;;
	cuda)    compiler_cmd=nvcc ;;
        *)       die "baselang $compiler_baselang not supported yet (spock-compiler-install)"
    esac

    local codegen_spec=$(spock-compiler-conditional-install-codegen "$code_generation")

    # Create the package YAML file that describes how to use this compiler.
    #
    # Compilers also have aliases to group them into other categories:
    #   c++-compiler | c-compiler | fortran-compiler | etc.
    #      Lists of compilers by their base language
    #   gnu++03-compiler | c++11-compiler | c89-compiler | etc.
    #      Lists of compilers by their specific language
    #   gnu-default-c++ | gnu-default-c | gnu-default-fortran | etc.
    #      Lists of compilers that parse the default language standard for a base language. For instance,
    #      gnu-default-c++-5.4.1 is an alias for gnu-gnu++03-5.4.1 but
    #      gnu-default-c++-6.3.0 is an alias for gnu-gnu++14-6.3.0
    #   default-c++ | default-c | default-fortran | etc.
    #      Vendor-agnostic group. While gnu-default-c++ is all the default (no -std=... switch) configurations
    #      of GNU C++ compilers, default-c++ also includes LLVM and Intel compilers.
    #
    # Code generation backends:
    #   In order to obtain an environment where all components (libraries, etc) are compiled with the same
    #   compatible code generation switches, compilers depend on specific "code-generation" pseudo-packages.
    #   For instance, if you want a 32-bit tool chain on a 64-bit machine, you'd use "m32-generation". Packages
    #   that directly or indirectly depend on m32-generation would probably not be compatible with packages that
    #   were installed in 64-bit mode.  The following code-generation packages are available:
    #
    #      PackageName         Meaning
    #      default-generation  Default code generation for this architecture.
    #      m32-generation      Compile with "-m32" to generate 32-bit installations.
    #
    #   All specific code generation package use the alias "code-generation". For instance, you can see what
    #   code generation capabilities are available by running "spock-ls code-generation".
    (
        echo "package:  '${compiler_vendor}-${compiler_lang}'"
        echo "version:  '$compiler_version'"
        echo "aliases:"
        echo "  - '${compiler_baselang}-compiler'"
        echo "  - '${compiler_lang}-compiler'"
        if [ "$#" = 0 ]; then # default language (the language when no language option is specified)
            echo "  - '${compiler_vendor}-default-${compiler_baselang}'"
            echo "  - 'default-$compiler_baselang'"
        fi
        echo "timestamp: '$(date --utc '+%Y-%m-%d %H:%M:%S')'"

        echo "dependencies:"
        echo "  - '$SPOCK_SPEC'"
        echo "  - '$collection_spec'"
        echo "  - '$codegen_spec'"

        bl=$(spock-compiler-base-language-var "$compiler_baselang")
        echo "environment:"
        echo "  PATH: '$package_bindir'"
        echo "  ${bl}_VENDOR:   '$compiler_vendor'"
        echo "  ${bl}_LANGUAGE: '$compiler_lang'"
        echo "  ${bl}_VERSION:  '$compiler_version'"
        echo "  ${bl}_ROOT:     '$package_root'"
        echo "  ${bl}_COMPILER: '$package_root/bin/$compiler_cmd'"
        [ "$compiler_libdirs" != "" ] &&
            echo "  LD_RUN_PATH: '$compiler_libdirs'"
    ) >"$package_yaml"

    echo "$package_spec"
    return 0
}

# Just like spock-compiler-install except this does nothing if the compiler appears to already be installed.
spock-compiler-conditional-install() {
    local compiler_quad="$1" collection_spec="$2" code_generation="$3" executable="$4"; shift 4 # compiler args

    local compiler_vendor=$(echo "$compiler_quad" |cut -d: -f1)
    local compiler_baselang=$(echo "$compiler_quad" |cut -d: -f2 -s)
    local compiler_lang=$(echo "$compiler_quad" |cut -d: -f3 -s)
    local compiler_version=$(echo "$compiler_quad" |cut -d: -f4 -s)

    [ -n "$compiler_quad"     ] || die "no compiler quad"
    [ -n "$compiler_vendor"   ] || die "no compiler vendor in quad $compiler_quad"
    [ -n "$compiler_baselang" ] || die "no compiler baselang in quad $compiler_quad"
    [ -n "$compiler_lang"     ] || die "no compiler lang in quad $compiler_quad"
    [ -n "$compiler_version"  ] || die "no compiler version in quad $compiler_quad"
    [ -n "$collection_spec"   ] || die "no compiler collection specification"
    [ -n "$executable"        ] || die "no compiler executable specified"

    # See if we've already installed this compiler. We do that by looking to see if there's a compiler with the
    # same spec and whose real executable is the same.
    local compiler_spec="${compiler_vendor}-${compiler_lang}${code_generation}=${compiler_version}"
    local other_specs=($("$SPOCK_BINDIR/spock-ls" -1 "$compiler_spec" 2>/dev/null))
    local real_exe=$(spock-realpath "$exe")
    local other_spec
    for other_spec in "${other_specs[@]}"; do
        local other_hash="${other_spec#*@}"
        local other_exe=$("$SPOCK_OPTDIR/$other_hash/$compiler_lang/bin/spock-compiler" --spock-exe 2>/dev/null)
        local other_real_exe=$(spock-realpath "$other_exe")
        [ "$other_real_exe" = "$real_exe" ] && return 0
    done

    spock-compiler-install "$compiler_quad" "$collection_spec" "$code_generation" "$executable" "$@"
}

# Given a compiler collection, base language, and command-line, conditionally install that compiler. Returns zero on
# success and echos the compiler specification that was installed.
spock-compiler-conditional-install-language() {
    local collection_spec="$1" compiler_baselang="$2" code_generation="$3" executable="$4"; shift 4 # compiler args
    local cg_switches=$(spock-compiler-code-generation-switches "$code_generation")

    local quad=$("$SPOCK_SCRIPTS/impl/detect-compiler-characteristics" --quad --baselang="$compiler_baselang" \
                                                                       "$executable" $cg_switches "$@")

    [ -n "$quad" ] && spock-compiler-conditional-install "$quad" "$collection_spec" "$code_generation" "$executable" "$@"
}

# Conditionally create a compiler collection package for compilers that are installed on this operating system. Echos
# the full spec of the collection regardless of whether it already existed or was created.
spock-compiler-conditional-install-collection() {
    local collection_type="$1" collection_vendor="$2" collection_version="$3"

    local collection_name="${collection_vendor}-${collection_type}"
    local collection_spec=$("$SPOCK_BINDIR/spock-ls" -1 "${collection_name}=${collection_version}" 2>/dev/null)
    if [ "$collection_spec" = "" ]; then
        while true; do
            local collection_hash=$(echo "$RANDOM" |sha1sum |cut -c1-8)
            local collection_yaml="$SPOCK_OPTDIR/$collection_hash.yaml"
            local collection_root="$SPOCK_OPTDIR/$collection_hash"
            local collection_spec="${collection_name}=${collection_version}@${collection_hash}"
            [ ! -e "$collection_yaml" -a ! -e "$collection_root" ] && break
        done

        mkdir -p "$collection_root" || exit 1
        (
            echo "package:      '$collection_name'"
            echo "version:      '$collection_version'"

	    # nVidia CUDA compilers don't belong to "compiler-collection" because we need to be
	    # able to use CUDA compilers in concert with other possibly incompatible compilers.
            echo -n "aliases:      [ '$collection_vendor-compilers'"
	    [ "$collection_vendor" != "nvidia" ] && echo -n ", compiler-collection"
	    echo "]"

            echo "dependencies: [ '$SPOCK_SPEC' ]"
            echo "timestamp: '$(date --utc '+%Y-%m-%d %H:%M:%S')'"
        ) >"$collection_yaml"
    fi
    echo "$collection_spec"
    return 0
}

# Conditionally create a code-generation package.  $code_generation is a list of zero or more code generation names
# each starting with a hyphen, and in any order.  This function emits a code generation spec on standard output, creating
# it if necessary.
spock-compiler-conditional-install-codegen() {
    local code_generation="$1"
    local generation_name=

    if [ "$code_generation" = "" ]; then
        generation_name="default-generation"
    else
        # Normalize the code generation names. E.g., "-m32-debug" will become "debug-m32-generation".
        code_generation=$(echo "$code_generation" |tr '-' '\n' |sort |tr '\n' '-')
        code_generation="${code_generation%-}"
        generation_name="${code_generation#-}-generation"
    fi

    # Given the generation name, such as "debug-m32-generation", find a spec that matches. These always have the
    # same version number as spock itself.
    local generation_spec=$("$SPOCK_BINDIR/spock-ls" -1 "${generation_name}=$SPOCK_VERSION" 2>/dev/null)
    if [ "$generation_spec" = "" ]; then
        while true; do
            local generation_hash=$(echo "$RANDOM" |sha1sum |cut -c1-8)
            local generation_yaml="$SPOCK_OPTDIR/$generation_hash.yaml"
            local generation_root="$SPOCK_OPTDIR/$generation_hash"
            local generation_spec="${generation_name}=${SPOCK_VERSION}@${generation_hash}"
            [ ! -e "$generation_yaml" -a ! -e "$generation_root" ] && break;
        done
        mkdir -p "$generation_root" || exit 1
        (
            echo "package:      '$generation_name'"
            echo "version:      '$SPOCK_VERSION'"
            echo "aliases:      [ code-generation ]"
            echo "dependencies: [ '$SPOCK_SPEC' ]"
            echo "timestamp:    '$(date --utc '+%Y-%m-%d %H:%M:%S')'"
        ) >"$generation_yaml"
    fi
    echo "$generation_spec"
    return 0
}

# Given a program name, try to determine if it's a compiler, and what type of compiler. Then install the appropriate
# compiler language packages, such as c++03, c++11, etc.  If successful, echo the names of the packages that were
# installed.  The collection argument is either "compilers" or "system-compilers", or it can be a collection
# specification with a hash number, with the first representing compilers that Spock downloaded and installed, and the
# second representing compilers that are already installed on this operating system.
spock-compiler-install-program() {
    local collection="$1" compiler_baselang="$2" exe=$(type -p "$3")
    [ "$exe" = "" ] && return 1

    # Avoid running ccache wrappers
    if [ "${cmd/ccache/}" != "$cmd" ]; then
        echo "$arg0: refusing to install ccache-managed compiler $exe" >&2
        return 1
    fi

    # Do not install if this is a spock-installed compiler already
    if "$exe" --spock-triplet </dev/null 2>/dev/null; then
        echo "$arg0: refusing to install an already-installed compiler: $exe" >&2
        return 1
    fi

    # Gather information about this compiler
    local quad=$("$SPOCK_SCRIPTS/impl/detect-compiler-characteristics" \
                     --quad --baselang="$compiler_baselang" "$exe" 2>/dev/null)
    [ "$quad" = "" ] && return 1
    local compiler_vendor=$(echo "$quad"   |cut -d: -f1)
    local compiler_baselang=$(echo "$quad" |cut -d: -f2 -s)
    local compiler_lang=$(echo "$quad"     |cut -d: -f3 -s)
    local compiler_version=$(echo "$quad"  |cut -d: -f4 -s)

    # Figure out which collection it belongs to and create that collection if necessary.
    local collection_spec
    if [ "$collection" != "${collection%@*}" ]; then
        local test_spec=$("$SPOCK_BINDIR/spock-ls" -1 "$collection" 2>/dev/null)
        [ "$collection" == "$test_spec" ] || die "compiler collection spec is malformed or missing: $collection"
        collection_spec="$collection"
    else
        collection_spec=$(spock-compiler-conditional-install-collection \
                              "$collection" "$compiler_vendor" "$compiler_version")
    fi
    [ "$collection_spec" = "" ] && return 1

    # Most compilers can handle more than one language, so install any that work.
    case "$compiler_vendor:$compiler_baselang" in
        gnu:c++|llvm:c++)
            spock-compiler-conditional-install-language "$collection_spec" c++ ""     "$exe"
            spock-compiler-conditional-install-language "$collection_spec" c++ "-m32" "$exe"
            spock-compiler-conditional-install-language "$collection_spec" c++ ""     "$exe" -std=c++03
            spock-compiler-conditional-install-language "$collection_spec" c++ "-m32" "$exe" -std=c++03
            spock-compiler-conditional-install-language "$collection_spec" c++ ""     "$exe" -std=c++11
            spock-compiler-conditional-install-language "$collection_spec" c++ "-m32" "$exe" -std=c++11
            spock-compiler-conditional-install-language "$collection_spec" c++ ""     "$exe" -std=c++14
            spock-compiler-conditional-install-language "$collection_spec" c++ "-m32" "$exe" -std=c++14
            spock-compiler-conditional-install-language "$collection_spec" c++ ""     "$exe" -std=c++17
            spock-compiler-conditional-install-language "$collection_spec" c++ "-m32" "$exe" -std=c++17
            spock-compiler-conditional-install-language "$collection_spec" c++ ""     "$exe" -std=gnu++03
            spock-compiler-conditional-install-language "$collection_spec" c++ "-m32" "$exe" -std=gnu++03
            spock-compiler-conditional-install-language "$collection_spec" c++ ""     "$exe" -std=gnu++11
            spock-compiler-conditional-install-language "$collection_spec" c++ "-m32" "$exe" -std=gnu++11
            spock-compiler-conditional-install-language "$collection_spec" c++ ""     "$exe" -std=gnu++14
            spock-compiler-conditional-install-language "$collection_spec" c++ "-m32" "$exe" -std=gnu++14
            spock-compiler-conditional-install-language "$collection_spec" c++ ""     "$exe" -std=gnu++17
            spock-compiler-conditional-install-language "$collection_spec" c++ "-m32" "$exe" -std=gnu++17
            ;;

        gnu:c|llvm:c)
            spock-compiler-conditional-install-language "$collection_spec" c ""     "$exe"
            spock-compiler-conditional-install-language "$collection_spec" c "-m32" "$exe"
            spock-compiler-conditional-install-language "$collection_spec" c ""     "$exe" -std=iso9899:1990
            spock-compiler-conditional-install-language "$collection_spec" c "-m32" "$exe" -std=iso9899:1990
            spock-compiler-conditional-install-language "$collection_spec" c ""     "$exe" -std=iso9899:199409
            spock-compiler-conditional-install-language "$collection_spec" c "-m32" "$exe" -std=iso9899:199409
            spock-compiler-conditional-install-language "$collection_spec" c ""     "$exe" -std=iso9899:1999
            spock-compiler-conditional-install-language "$collection_spec" c "-m32" "$exe" -std=iso9899:1999
            spock-compiler-conditional-install-language "$collection_spec" c ""     "$exe" -std=iso9899:2011
            spock-compiler-conditional-install-language "$collection_spec" c "-m32" "$exe" -std=iso9899:2011
            spock-compiler-conditional-install-language "$collection_spec" c ""     "$exe" -std=iso9899:2018
            spock-compiler-conditional-install-language "$collection_spec" c "-m32" "$exe" -std=iso9899:2018
            spock-compiler-conditional-install-language "$collection_spec" c ""     "$exe" -std=gnu90
            spock-compiler-conditional-install-language "$collection_spec" c "-m32" "$exe" -std=gnu90
            spock-compiler-conditional-install-language "$collection_spec" c ""     "$exe" -std=gnu99
            spock-compiler-conditional-install-language "$collection_spec" c "-m32" "$exe" -std=gnu99
            spock-compiler-conditional-install-language "$collection_spec" c ""     "$exe" -std=gnu11
            spock-compiler-conditional-install-language "$collection_spec" c "-m32" "$exe" -std=gnu11
            spock-compiler-conditional-install-language "$collection_spec" c ""     "$exe" -std=gnu18
            spock-compiler-conditional-install-language "$collection_spec" c "-m32" "$exe" -std=gnu18
            ;;

	nvidia:cuda)
	    spock-compiler-conditional-install-language "$collection_spec" cuda "" "$exe"
	    ;;

        *:c++)
            spock-compiler-conditional-install-language "$collection_spec" c++ ""     "$exe"
            spock-compiler-conditional-install-language "$collection_spec" c++ "-m32" "$exe"
            spock-compiler-conditional-install-language "$collection_spec" c++ ""     "$exe" -std=c++03
            spock-compiler-conditional-install-language "$collection_spec" c++ "-m32" "$exe" -std=c++03
            spock-compiler-conditional-install-language "$collection_spec" c++ ""     "$exe" -std=c++11
            spock-compiler-conditional-install-language "$collection_spec" c++ "-m32" "$exe" -std=c++11
            spock-compiler-conditional-install-language "$collection_spec" c++ ""     "$exe" -std=c++14
            spock-compiler-conditional-install-language "$collection_spec" c++ "-m32" "$exe" -std=c++14
            spock-compiler-conditional-install-language "$collection_spec" c++ ""     "$exe" -std=c++17
            spock-compiler-conditional-install-language "$collection_spec" c++ "-m32" "$exe" -std=c++17
            ;;

        *:c)
            spock-compiler-conditional-install-language "$collection_spec" c ""     "$exe"
            spock-compiler-conditional-install-language "$collection_spec" c "-m32" "$exe"
            spock-compiler-conditional-install-language "$collection_spec" c ""     "$exe" -std=iso9899:1990
            spock-compiler-conditional-install-language "$collection_spec" c "-m32" "$exe" -std=iso9899:1990
            spock-compiler-conditional-install-language "$collection_spec" c ""     "$exe" -std=iso9899:199409
            spock-compiler-conditional-install-language "$collection_spec" c "-m32" "$exe" -std=iso9899:199409
            spock-compiler-conditional-install-language "$collection_spec" c ""     "$exe" -std=iso9899:1999
            spock-compiler-conditional-install-language "$collection_spec" c "-m32" "$exe" -std=iso9899:1999
            spock-compiler-conditional-install-language "$collection_spec" c ""     "$exe" -std=iso9899:2011
            spock-compiler-conditional-install-language "$collection_spec" c "-m32" "$exe" -std=iso9899:2011
            spock-compiler-conditional-install-language "$collection_spec" c ""     "$exe" -std=iso9899:2018
            spock-compiler-conditional-install-language "$collection_spec" c "-m32" "$exe" -std=iso9899:2018
            ;;

        *)
            spock-compiler-conditional-install-language "$collection_spec" fortran ""     "$exe"
            spock-compiler-conditional-install-language "$collection_spec" fortran "-m32" "$exe"
            ;;
    esac
    return 0
}
