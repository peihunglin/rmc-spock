# RMC/Spock

RMC/Spock is the ROSE meta configuration system built on top of Spock package management.

The RMC layer is a few small shell scripts that allow a developer to
specify what features they need in a ROSE build tree, and the Spock
layer is responsible for finding a solution that satisfies the request
and configuring a shell environment to use that solution.

# Documentation for ROSE developers

Search for RMC-2 in our confluence pages.

# Quick start for users wanting to compile ROSE

First, consult the templates
[here](https://github.com/matzke1/rose-docker/tree/master/templates)
for hints about how to install prerequisites. Then...

	# Install ROSE Meta Config (RMC) / Spock
	git clone https://github.com/matzke1/rmc-spock
	(cd rmc-spock && ./scripts/bootstrap --upgrade)
	rm -rf rmc-spock # optional
	export PATH="$HOME/.spock/bin:$PATH"

	# Download ROSE and configure for binary analysis
	git clone https://github.com/rose-compiler/rose-develop rose
	mkdir rose/_build # arbitrary
	cd rose/_build
	rmc init --project=binaries .. # --project is optional
	rmc config
	rmc make install-rose-library
	rmc  -C projects/BinaryAnalysisTools make install

