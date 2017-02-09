# Shell functions for helping with package installation

# Default parallelism when compiling.
if [ "$PARALLELISM" = "" ]; then
    PARALLELISM=$(sed -n '/^processor[ \t]\+:/p' </proc/cpuinfo |wc -l)
    [ "$PARALLELISM" = "" -o "$PARALLELISM" -eq 0 ] && PARALLELISM=1
fi

################################################################################
# Functions to aid package configuration

# Generate compiler "-L" switches for all package library directories
spock-compiler-libdirs() {
    if [ -n "$ALL_LIBDIRS" ]; then
	echo "$ALL_LIBDIRS" |sed 's/^\|:\+/ -L/g'
    fi
}

# Generate compiler "-I" switches for all package include directories
spock-compiler-incdirs() {
    if [ -n "$ALL_INCDIRS" ]; then
	echo "$ALL_INCDIRS" |sed 's/^\|:\+/ -I/g'
    fi
}

################################################################################
# Functions to set up the YAML "environment" section of the installed.yaml file.
if [ "$PACKAGE_ACTION" = "install" ]; then
    INSTALLED_YAML="$(pwd)/installed.yaml"
    (
	echo "environment:"
	echo "    ${PACKAGE_NAME_UC}_ROOT:    '$PACKAGE_ROOT'"
	echo "    ${PACKAGE_NAME_UC}_VERSION: '$PACKAGE_VERSION'"
    ) >"$INSTALLED_YAML"
fi

# Arrange to insert values into an environment variable when the package is used
spock-export() {
    local varname="$1"; shift # values
    if [ "$varname" = "" -o "$#" -eq 0 ]; then
	echo "spock-export: error: no variable or value" >&2
	return 1
    fi

    local value="$1" ; shift
    local v
    for v in "$@"; do
	value="$value:$v"
    done
    if [ "$value" = "" ]; then
	echo "spock-export: error: empty value for $varname" >&2
	return 1
    fi

    echo "    '$varname': '$value'" >>"$INSTALLED_YAML"
    return 0
}

# This gets called automatically at the end of all generated shell scripts
spock-finalize() {
    set +x
    if [ "$PACKAGE_ACTION" = "install" ]; then
	(
	    # Libraries
	    if [ -d "$PACKAGE_ROOT/lib" ]; then
		echo "    ${PACKAGE_NAME_UC}_LIBDIRS: '$PACKAGE_ROOT/lib'"
		echo "    ALL_LIBDIRS: '$PACKAGE_ROOT/lib'"

		# If there appear to be shared libraries in $PACKAGE_ROOT/lib, then add them
		# to the LD_LIBRARY_PATH
		if [ -n "$(ls $PACKAGE_ROOT/lib/*.so 2>/dev/null)" ]; then
		    echo "    LD_LIBRARY_PATH:            '$PACKAGE_ROOT/lib'"
		fi
	    fi

	    # Include files
	    if [ -d "$PACKAGE_ROOT/include" ]; then
		echo "    ${PACKAGE_NAME_UC}_INCDIRS: '$PACKAGE_ROOT/include'"
		echo "    ALL_INCDIRS: '$PACKAGE_ROOT/include'"
	    fi

	    # Executables
	    if [ -d "$PACKAGE_ROOT/bin" ]; then
		echo "    PATH: '$PACKAGE_ROOT/bin'"
	    fi
	) >>"$INSTALLED_YAML"
    fi
}

################################################################################
# Print some final information to help debug installation failures.
#"$SPOCK_BIN/spock-employed"
