# Shell functions for helping with package installation

# Default parallelism when compiling.
if [ "$PARALLELISM" = "" ]; then
    PARALLELISM=$(sed -n '/^processor[ \t]\+:/p' </proc/cpuinfo |wc -l)
    [ "$PARALLELISM" = "" -o "$PARALLELISM" -eq 0 ] && PARALLELISM=1
fi

################################################################################
# Functions to set up the YAML "environment" section of the installed.yaml file.
if [ "$PACKAGE_ACTION" = "install" ]; then
    INSTALLED_YAML="$(pwd)/installed.yaml"
    (
	echo "environment:"
	echo "    '${PACKAGE_NAME_UC}_ROOT':    '$PACKAGE_ROOT'"
	echo "    '${PACKAGE_NAME_UC}_VERSION': '$PACKAGE_VERSION'"
    ) >"$INSTALLED_YAML"
fi

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

################################################################################
# Print some final information to help debug installation failures.
#"$SPOCK_BIN/spock-employed"
