#!/bin/bash

arg0="${0##*/}"
dir0="${0%/*}"

# Portable wrapper around OS-specific version
os_name="$(hostname --short)"

# Installed version
[ -x "$dir0/$os_name/$arg0" ] && exec "$dir0/$os_name/$arg0" "$@"

# Is user asking us to install spock on this machine?
if [ "$arg0" = "install-spock" ]; then
    workdir=$(mktemp --tmpdir --directory)
    (
	set -e
	cd "$workdir"
	git clone https://github.com/matzke1/rmc-spock
	(cd rmc-spock && ./scripts/bootstrap.sh)
    ) || exit 1
    rm -rf "$workdir"
    exit 0
fi

(
    echo "$arg0: not installed on $os_name"
    echo "$arg0: install using these steps:"
    echo "$arg0:   \$ git clone https://github.com/matzke1/rmc-spock"
    echo "$arg0:   \$ (cd rmc-spock && ./scripts/bootstrap.sh)"
    echo "$arg0:   \$ rm -rf rmc-spock"
    echo "$arg0: or"
    echo "$arg0:   \$ $dir0/install-spock"
)
