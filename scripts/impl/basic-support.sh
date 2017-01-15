# Basic shell programming support

die() {
    echo "$arg0:" "$@" >&2
    exit 1
}

# The tempfile command is not always available, so reimplement it here if necessary.
tempfile() {
    local real_tempfile="$(which tempfile 2>/dev/null)"
    if [ -n "$real_tempfile" ]; then
	"$real_tempfile"
    else
	local tmpdir="${TMPDIR:-$TEMPDIR}"
	mkdir -p "${tmpdir:-/tmp}"
	echo "${tmpdir:-/tmp}/spock-$RANDOM$RANDOM$RANDOM"
    fi
}
