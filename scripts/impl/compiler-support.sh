# Shell functions for installing compilers

source "$SPOCK_SCRIPTS/impl/basic-support.sh" || exit 1
source "$SPOCK_SCRIPTS/impl/spock-realpath.sh" || exit 1

# Generate a suitable shell variable to describe a base language
spock-compiler-base-language-var() {
    local base_language="$1"
    case "$base_language" in
	c++) echo "CXX" ;;
	c) echo "C" ;;
	fortran) echo "FORTRAN" ;;
	java) echo "JAVA" ;;
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
    local compiler_quad="$1" collection_spec="$2" executable="$3"; shift 3 # compiler args

    local compiler_vendor=$(echo "$compiler_quad" |cut -d: -f1)
    local compiler_baselang=$(echo "$compiler_quad" |cut -d: -f2)
    local compiler_lang=$(echo "$compiler_quad" |cut -d: -f3)
    local compiler_version=$(echo "$compiler_quad" |cut -d: -f4)

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
	local package_spec="$compiler_vendor-$compiler_lang=$compiler_version@$package_hash"
	[ ! -e "$package_yaml" -a ! -e "$package_root" ] && break
    done
    mkdir -p "$package_bindir" || exit 1

    # Create the compiler executables in the package's "bin" directory. This directory also needs the YAML configuration
    # file so the spock-compiler wrapper knows how to run the real compiler.
    (
	set -e
	cd "$package_bindir"
	cp "$SPOCK_BINDIR/spock-compiler" "."

	(
	    echo "#!/bin/bash"
	    echo "echo \"\${0##*/} is not a valid $compiler_vendor $compiler_lang compiler\" >&2"
	    echo "exit 1"
	) >not-a-compiler
	chmod 755 not-a-compiler

	"$SPOCK_SCRIPTS/impl/detect-compiler-characteristics" \
	    --yaml --baselang="$compiler_baselang" "$executable" "$@" >"$package_bindir/compiler.yaml"

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
	esac
    ) || exit 1

    local compiler_cmd=
    case "$compiler_baselang" in
	c++)     compiler_cmd=c++ ;;
	c)       compiler_cmd=cc  ;;
	fortran) compiler_cmd=fc  ;;
	*)       die "baselang $compiler_baselang not supported yet (spock-compiler-install)"
    esac
    
    # Create the package YAML file that describes how to use this compiler.
    (
	echo "package:  '$compiler_vendor-$compiler_lang'"
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

	bl=$(spock-compiler-base-language-var "$compiler_baselang")
	echo "environment:"
	echo "  PATH: '$package_bindir'"
	echo "  ${bl}_VENDOR:   '$compiler_vendor'"
	echo "  ${bl}_LANGUAGE: '$compiler_lang'"
	echo "  ${bl}_VERSION:  '$compiler_version'"
	echo "  ${bl}_ROOT:     '$package_root'"
	echo "  ${bl}_COMPILER: '$package_root/bin/$compiler_cmd'"
    ) >"$package_yaml"

    echo "$package_spec"
    return 0
}

# Just like spock-compiler-install except this does nothing if the compiler appears to already be installed.
spock-compiler-conditional-install() {
    local compiler_quad="$1" collection_spec="$2" executable="$3"; shift 3 # compiler args

    local compiler_vendor=$(echo "$compiler_quad" |cut -d: -f1)
    local compiler_baselang=$(echo "$compiler_quad" |cut -d: -f2)
    local compiler_lang=$(echo "$compiler_quad" |cut -d: -f3)
    local compiler_version=$(echo "$compiler_quad" |cut -d: -f4)

    [ -n "$compiler_quad"     ] || die "no compiler quad"
    [ -n "$compiler_vendor"   ] || die "no compiler vendor in quad $compiler_quad"
    [ -n "$compiler_baselang" ] || die "no compiler baselang in quad $compiler_quad"
    [ -n "$compiler_lang"     ] || die "no compiler lang in quad $compiler_quad"
    [ -n "$compiler_version"  ] || die "no compiler version in quad $compiler_quad"
    [ -n "$collection_spec"   ] || die "no compiler collection specification"
    [ -n "$executable"        ] || die "no compiler executable specified"

    # See if we've already installed this compiler. We do that by looking to see if there's a compiler with the
    # same spec and whose real executable is the same.
    local compiler_spec="${compiler_vendor}-${compiler_lang}=${compiler_version}"
    local other_specs=($("$SPOCK_BINDIR/spock-ls" -1 "$compiler_spec" 2>/dev/null))
    local real_exe=$(spock-realpath "$exe")
    local other_spec
    for other_spec in "${other_specs[@]}"; do
	local other_hash="${other_spec#*@}"
	local other_exe=$("$SPOCK_OPTDIR/$other_hash/$compiler_lang/bin/spock-compiler" --spock-exe 2>/dev/null)
	local other_real_exe=$(spock-realpath "$other_exe")
	[ "$other_real_exe" = "$real_exe" ] && return 0
    done

    spock-compiler-install "$compiler_quad" "$collection_spec" "$executable" "$@"
}

# Given a compiler collection, base language, and command-line, conditionally install that compiler. Returns zero on
# success and echos the compiler specification that was installed.
spock-compiler-conditional-install-language() {
    local collection_spec="$1" compiler_baselang="$2" executable="$3"; shift 3 # compiler args

    local quad=$("$SPOCK_SCRIPTS/impl/detect-compiler-characteristics" --quad --baselang="$compiler_baselang" \
								       "$executable" "$@")

    [ -n "$quad" ] && spock-compiler-conditional-install "$quad" "$collection_spec" "$executable" "$@"
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
	    echo "aliases:      [ '$collection_vendor-compilers', compiler-collection ]"
	    echo "dependencies: [ '$SPOCK_SPEC' ]"
	    echo "timestamp: '$(date --utc '+%Y-%m-%d %H:%M:%S')'"
	) >"$collection_yaml"
    fi
    echo "$collection_spec"
    return 0
}

# Given a program name, try to determine if it's a compiler, and what type of compiler. Then install the appropriate
# compiler language packages, such as c++03, c++11, etc.  If successful, echo the names of the packages that were
# installed.  The collection argument is either "compilers" or "system-compilers", or it can be a collection
# specification with a hash number, with the first representing compilers that Spock downloaded and installed, and the
# second representing compilers that are already installed on this operating system.
spock-compiler-install-program() {
    local collection="$1" compiler_baselang="$2" exe=$(which "$3")
    [ "$exe" = "" ] && return 1

    # Do not install if this is a spock-installed compiler already
    if "$exe" --spock-triplet 2>/dev/null; then
	echo "$arg0: refusing to install an already-installed compiler: $exe" >&2
	return 1
    fi

    # Gather information about this compiler
    local quad=$("$SPOCK_SCRIPTS/impl/detect-compiler-characteristics" \
		     --quad --baselang="$compiler_baselang" "$exe" 2>/dev/null)
    [ "$quad" = "" ] && return 1
    local compiler_vendor=$(echo "$quad"   |cut -d: -f1)
    local compiler_baselang=$(echo "$quad" |cut -d: -f2)
    local compiler_lang=$(echo "$quad"     |cut -d: -f3)
    local compiler_version=$(echo "$quad"  |cut -d: -f4)

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
	    spock-compiler-conditional-install-language "$collection_spec" c++ "$exe"
	    spock-compiler-conditional-install-language "$collection_spec" c++ "$exe" -std=c++03
	    spock-compiler-conditional-install-language "$collection_spec" c++ "$exe" -std=c++11
	    spock-compiler-conditional-install-language "$collection_spec" c++ "$exe" -std=c++14
	    spock-compiler-conditional-install-language "$collection_spec" c++ "$exe" -std=gnu++03
	    spock-compiler-conditional-install-language "$collection_spec" c++ "$exe" -std=gnu++11
	    spock-compiler-conditional-install-language "$collection_spec" c++ "$exe" -std=gnu++14
	    ;;

	gnu:c|llvm:c)
	    spock-compiler-conditional-install-language "$collection_spec" c "$exe"
	    spock-compiler-conditional-install-language "$collection_spec" c "$exe" -std=iso9899:1990
	    spock-compiler-conditional-install-language "$collection_spec" c "$exe" -std=iso9899:199409
	    spock-compiler-conditional-install-language "$collection_spec" c "$exe" -std=iso9899:1999
	    spock-compiler-conditional-install-language "$collection_spec" c "$exe" -std=iso9899:2011
	    spock-compiler-conditional-install-language "$collection_spec" c "$exe" -std=gnu90
	    spock-compiler-conditional-install-language "$collection_spec" c "$exe" -std=gnu99
	    spock-compiler-conditional-install-language "$collection_spec" c "$exe" -std=gnu11
	    ;;

	*:c++)
	    spock-compiler-conditional-install-language "$collection_spec" c++ "$exe"
	    spock-compiler-conditional-install-language "$collection_spec" c++ "$exe" -std=c++03
	    spock-compiler-conditional-install-language "$collection_spec" c++ "$exe" -std=c++11
	    spock-compiler-conditional-install-language "$collection_spec" c++ "$exe" -std=c++14
	    ;;

	*:c)
	    spock-compiler-conditional-install-language "$collection_spec" c "$exe"
	    spock-compiler-conditional-install-language "$collection_spec" c "$exe" -std=iso9899:1990
	    spock-compiler-conditional-install-language "$collection_spec" c "$exe" -std=iso9899:199409
	    spock-compiler-conditional-install-language "$collection_spec" c "$exe" -std=iso9899:1999
	    spock-compiler-conditional-install-language "$collection_spec" c "$exe" -std=iso9899:2011
	    ;;

	*)
	    spock-compiler-conditional-install-language "$collection_spec" fortran "$exe"
	    ;;
    esac
    return 0
}
