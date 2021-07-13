# Basic shell programming support

die() {
    echo "$arg0:" "error:" "$@" >&2
    exit 1
}

# The 'tempfile' command is not always available, so reimplement it here if necessary.
# Now 'tempfile' is deprecated, so use mktemp if available.
tempfile() {
    local real_mktemp="$(which mktemp 2>/dev/null)"
    if [ -n "$real_mktemp" ]; then
	"$real_mktemp"
    else
	local real_tempfile="$(which tempfile 2>/dev/null)"
	if [ -n "$real_tempfile" ]; then
	    "$real_tempfile"
	else
	    local tmpdir="${TMPDIR:-$TEMPDIR}"
	    mkdir -p "${tmpdir:-/tmp}"
	    echo "${tmpdir:-/tmp}/spock-$RANDOM$RANDOM$RANDOM"
	fi
    fi
}
