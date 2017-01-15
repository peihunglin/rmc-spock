#!/bin/bash
# This sets up the environment for Robb to run Spock directly from his Spock build tree.

# The usual location for Spock runtime is $HOME/.spock, but that's a small, fast filesystem on
# Robb's machine, so he uses ~/GS-CAD with is NFS mounted to a 16TB filesystem.
: ${SPOCK_ROOT=$HOME/GS-CAD/spock}
export SPOCK_ROOT

# The next directories normally default to be somewhere under $SPOCK_ROOT, but need to
# be adjusted when running Spock from its build tree.
: ${SPOCK_SOURCE:=$HOME/Spock}
export SPOCK_BINDIR=$SPOCK_SOURCE/_build
export SPOCK_PKGDIR=$SPOCK_SOURCE/lib/packages
export SPOCK_SCRIPTS=$SPOCK_SOURCE/scripts

# If your /tmp filesystem is not large enough, you can use something else. On Robb's
# machine, ~/junk is a multiple terabyte solid-state drive, which makes for fast compiling.
export SPOCK_BLDDIR=$HOME/junk

# Add Spock's bin directory to $PATH
eval $(path-adjust --prepend --move $SPOCK_BINDIR)
