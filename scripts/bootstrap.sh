#!/bin/bash
# This script tries to build Spock and its dependencies.
# You should invoke this script in the top of the Spock
# source tree as "./scripts/bootstrap.sh".
set -e
arg0="${0##*/}"

# LLNL intercepts all SSL connections with a self-signed certificate
wget() {
    command wget --no-check-certificate "$@"
}

#-------------------- Commandline parsing --------------------

prefix=
downloads=
upgrade=yes
boost_version=1.62.0
cmake_version=3.12.1
yamlcpp_version=0.5.3
sawyer_version=0.2.0
have_network=yes

while [ "$#" -gt 0 ]; do
    case "$1" in
	# Boost version number
	--boost=*)
	    boost_version="${1#--boost=}"
	    shift
	    ;;
	--boost)
	    boost_version="$2"
	    shift 2
	    ;;

	# CMake version
	--cmake=*)
	    cmake_version="${1#--cmake=}"
	    shift
	    ;;
	--cmake)
	    cmake_veresion="$2"
	    shift 2
	    ;;

	# If you already ran some other version of Spock, some things
	# might have been downloaded already. You can point this
	# script to that directory.
	--downloads=*)
	    downloads="${1#--downloads=}"
	    shift
	    ;;
	--downloads)
	    downloads="$2"
	    shift 2
	    ;;

	# Whether we have network access
	--network)
	    have_network=yes
	    shift
	    ;;
	--no-network)
	    have_network=
	    shift
	    ;;

	# Where to install Spock's files.
	--prefix=*)
	    prefix="${1#--prefix=}"
	    shift
	    ;;
	--prefix)
	    prefix="$2"
	    shift 2
	    ;;

	# Sawyer version
	--sawyer=*)
	    sawyer_version="${1#--sawyer=}"
	    shift
	    ;;
	--sawyer)
	    sawyer_version="$2"
	    shift 2
	    ;;

	# Upgrade to a new version of spock, such as when the OS is upgraded. This
	# will try to re-use as much as possible of the original installation.
	--upgrade)
	    upgrade=yes
	    shift
	    ;;
        --no-upgrade)
            upgrade=
            shift
            ;;

	# Yaml-cpp version
	--yamlcpp=*)
	    yamlcpp_version="${1#--yamlcpp=}"
	    shift
	    ;;
	--yamlcpp)
	    yamlcpp_version="$2"
	    shift 2
	    ;;

	--)
	    shift
	    break
	    ;;
	-*)
	    echo "$arg0: invalid swtich: $1" >&2
	    exit 1
	    ;;
	*)
	    echo "usage: $0 [--prefix=DIRECTORY] [--downloads=DIRECTORY]" >&2
	    exit 1
	    ;;
    esac
done

if [ -n "$SPOCK_EMPLOYED" ]; then
    echo "$arg0: error: you cannot run this script from inside a spock shell" >&2
    echo "$arg0: info: exit this shell, then try again" >&2
    echo "$arg0: info: you're at shell level $[SHLVL-1]" >&2
    exit 0
fi

if [ "$prefix" = "" ]; then
    prefix="$HOME/.spock"
    echo "$arg0: assuming --prefix=$prefix"
fi
if [ "$downloads" = "" ]; then
    downloads="$prefix/var/downloads"
    echo "$arg0: assuming --downloads=$downloads"
fi

mkdir -p "$downloads" || exit 1

#-------------------- Basic Setup --------------------
if [ ! -e src/Spock/Spock.h -o ! -e scripts/bootstrap.sh ]; then
    echo "$arg0: error: invoke this script from the top of the Spock source tree" >&2
    exit 1
fi
if ! mkdir _build; then
    echo "$arg0: please delete your old '_build' directory first" >&2
    exit 1
fi
[ "$SPOCK_HOSTNAME" = "" ] && SPOCK_HOSTNAME=$(hostname --short)

if [ -n "$upgrade" ]; then
    echo "$arg0: removing old installation for $SPOCK_HOSTNAME"
    rm -rf "$prefix/bin/$SPOCK_HOSTNAME"
    rm -rf "$prefix/lib/$SPOCK_HOSTNAME"
    rm -rf "$prefix/dependencies/$SPOCK_HOSTNAME"
    rm -rf "$prefix/var/installed/$SPOCK_HOSTNAME"
fi


# Figure out what compiler to use.
cxx_quad=
cxx_exe=
if [ "$(which g++)" != "" ]; then
    cxx_exe="$(which g++)"
    cxx_quad=$(./scripts/impl/detect-compiler-characteristics --quad "$cxx_exe")
    c_exe="$(which gcc)"
    c_quad=$(./scripts/impl/detect-compiler-characteristics --quad "$c_exe")
elif [ "$(which clang++)" != "" ]; then
    echo "$arg0: LLVM compilers not yet tested" >&2
    exit 1
elif [ "$(which icpc)" != "" ]; then
    echo "$arg0: Intel compilers not yet tested" >&2
    exit 1
fi

if [ "$cxx_quad" = "" ]; then
    echo "$arg0: do you even have a C++ compiler?" >&2
    exit 1
fi
cxx_vendor=$(echo "$cxx_quad" |cut -d: -f1)
cxx_version=$(echo "$cxx_quad" |cut -d: -f4 -s)

# Check version number (must be >= 4.4.7)
too_old=
if [ "$cxx_vendor" = "gnu" ]; then
    cxx_version_major=$(echo "$cxx_version" |cut -d. -f1)
    cxx_version_minor=$(echo "$cxx_version" |cut -d. -f2 -s)
    cxx_version_patch=$(echo "$cxx_version" |cut -d. -f3 -s)
    if [ "$cxx_version_major" -lt 4 ]; then
	too_old=yes
    elif [ "$cxx_version_major" = "4" -a "$cxx_version_minor" -lt 4 ]; then
	too_old=yes
    elif [ "$cxx_version_major" = "4" -a "$cxx_version_minor" = "4" -a "$cxx_version_patch" -lt 7 ]; then
	too_old=yes
    fi
fi
if [ -n "$too_old" ]; then
    echo "$arg0: your compiler is too old: $cxx_vendor=$cxx_version" >&2

    # If this is an LC machine, try upgrading to a reasonably recent GCC compiler
    if [ -e "/usr/local/tools/dotkit/init.sh" ]; then
	best_gcc_49=$(source /usr/local/tools/dotkit/init.sh >/dev/null; \
		      use -l |sed -n 's/^ *\(gcc-4.9.[0-9]p\) \+.*/\1/p' |sort -r |head -1)
	if [ "$best_gcc_49" != "" ]; then
	    echo "$arg0: try uprading by running 'use $best_gcc_49'" >&2
	fi
    fi
    exit 1
fi
echo "$arg0: compiling RMC/Spock and dependencies with $cxx_quad"

# How many threads can we use for compiling?
ncpus=$(sed -n '/^processor[ \t]\+:/p' </proc/cpuinfo |wc -l)
[ "$ncpus" = "" -o "$ncpus" -eq 0 ] && ncpus=1


#-------------------- Boost --------------------
boost_version_u="$(echo "$boost_version" |tr . _)"
: ${boost_url:=http://sourceforge.net/projects/boost/files/boost/$boost_version/boost_${boost_version_u}.tar.bz2/download}
boost_libs=chrono,date_time,filesystem,iostreams,program_options,random,regex,serialization,signals,system,thread,wave
boost_root="$prefix/dependencies/$SPOCK_HOSTNAME/boost"

if [ ! -d "$boost_root" ]; then
    (
        set -ex
	mkdir -p _build/boost
        cd _build/boost
        if [ -e "$downloads/boost-${boost_version}.tar.gz" ]; then
            tar xf "$downloads/boost-${boost_version}.tar.gz"
            mv download boost_${boost_version_u}
	elif [ -e "$downloads/boost_${boost_version_u}.tar.bz2" ]; then
	    tar xf "$downloads/boost_${boost_version_u}.tar.bz2"
        elif [ -n "$have_network" ]; then
            wget -O - "$boost_url" |tar xjf -
	else
	    echo "$arg0: cannot find boost-${boost_version}, and no network" >&2
	    exit 1
        fi
	if [ -n "$downloads" -a ! -e "$downloads/boost-${boost_version}.tar.gz" ]; then
	    ln -s boost_$boost_version_u download
	    tar cf - download/. |gzip -9 >"$downloads/boost-${boost_version}.tar.gz"
	    rm download
	fi
        cd boost_${boost_version_u}

	boost_cxx_vendor=
	case "$cxx_vendor" in
	    gnu) boost_cxx_vendor=gcc ;;
	    llvm) boost_cxx_vendor=clang ;;
	    *) boost_cxx_vendor="$cxx_vendor" ;;
	esac

        echo "using $boost_cxx_vendor : : $cxx_exe ;" >user-config.jam
	export BOOST_BUILD_PATH="$(pwd)"
        ./bootstrap.sh --prefix="$boost_root" --with-libraries="$boost_libs" --with-toolset="$boost_cxx_vendor"
        ./b2 --prefix="$boost_root" -sNO_BZIP2=1 toolset="$boost_cxx_vendor" -j$ncpus
        ./b2 --prefix="$boost_root" -sNO_BZIP2=1 toolset="$boost_cxx_vendor" install
    )
    rm -rf _build/boost_${boost_version_u}
fi

#-------------------- CMake --------------------
#cmake_version_2=$(echo "$cmake_version" |cut -d. -f1-2)
#: ${cmake_url:=https://cmake.org/files/v${cmake_version_2}/cmake-${cmake_version}.tar.gz}
: ${cmake_url:=https://github.com/Kitware/CMake/releases/download/v${cmake_version}/cmake-${cmake_version}.tar.gz}
cmake_root="$prefix/dependencies/$SPOCK_HOSTNAME/cmake"
if [ ! -d "$cmake_root" ]; then
    (
	set -ex
	cd _build

	if [ -e "$downloads/cmake-${cmake_version}.tar.gz" ]; then
	    tar xf "$downloads/cmake-${cmake_version}.tar.gz"
	    mv download cmake-src
	elif [ -n "$have_network" ]; then
	    wget -O - "$cmake_url" |tar xzf -
	    mv cmake-${cmake_version} cmake-src
	else
	    echo "$arg0: cannot find cmake-${cmake_version}, and no network" >&2
	    exit 1
	fi
	if [ -n "$downloads" -a ! -e "$downloads/cmake-${cmake_version}.tar.gz" ]; then
	    ln -s cmake-src download
	    tar cf - download/. |gzip -9 >"$downloads/cmake-${cmake_version}.tar.gz"
	    rm download
	fi
	cd cmake-src

        # Work around CMake issue 18057?
        fix_libtinfo=
        if [ -r /etc/os-release ]; then
            if grep "Red Hat" /etc/os-release >/dev/null; then
                if [ -e /usr/lib/libtinfo.so -a ! -e /usr/lib64/libtinfo.so -a -e /usr/lib64/libtinfo.so.5 ]; then
                    fix_libtinfo="-- -DBUILD_CursesDialog=OFF"
                fi
            fi
        fi
	if [ -r /etc/redhat-release ]; then
	    fix_libtinfo="-- -DBUILD_CursesDialog=OFF"
	fi

        ./bootstrap --parallel=$ncpus --prefix="$cmake_root" $fix_libtinfo
        make -j$ncpus
        make install
    )
    rm -rf _build/cmake-src
fi

#-------------------- Yaml-cpp --------------------
: ${yamlcpp_url:=https://github.com/jbeder/yaml-cpp}

yamlcpp_root="$prefix/dependencies/$SPOCK_HOSTNAME/yamlcpp"
if [ ! -d "$yamlcpp_root" ]; then
    (
        set -ex
	cd _build

	if [ -e "$downloads/yamlcpp-${yamlcpp_version}.tar.gz" ]; then
	    tar xf "$downloads/yamlcpp-${yamlcpp_version}.tar.gz"
	    mv download yamlcpp-src
	elif [ -n "$have_network" ]; then
            git clone -b release-${yamlcpp_version} "$yamlcpp_url" yamlcpp-src
            (cd yamlcpp-src && git checkout -b r-${yamlcpp_version} release-${yamlcpp_version})
	    if [ -n "$downloads" ]; then
		ln -s yamlcpp-src download
		tar cf - download/. |gzip -9 >"$downloads/yamlcpp-${yamlcpp_version}.tar.gz"
		rm download
	    fi
	else
	    echo "$arg0: cannot find yamlcpp-${yamlcpp_version}, and no network" >&2
	    exit 1
	fi

        mkdir yamlcpp-bld
        cd yamlcpp-bld
        "$cmake_root/bin/cmake" ../yamlcpp-src \
              -DCMAKE_C_COMPILER="$c_exe" \
              -DCMAKE_CXX_COMPILER="$cxx_exe" \
              -DBUILD_SHARED_LIBS=Yes \
              -DBOOST_ROOT="$boost_root" \
              -DCMAKE_INSTALL_PREFIX="$yamlcpp_root"
        make -j$ncpus install
    )
    rm -rf _build/yamlcpp-bld _build/yamlcpp-src
fi

#-------------------- Sawyer --------------------
: ${sawyer_url:=https://github.com/matzke1/sawyer}

sawyer_root="$prefix/dependencies/$SPOCK_HOSTNAME/sawyer"
if [ ! -d "$sawyer_root" ]; then
    (
        set -ex
        cd _build

	if [ -e "$downloads/sawyer-${sawyer_version}.tar.gz" ]; then
	    tar xf "$downloads/sawyer-${sawyer_version}.tar.gz"
	    mv download sawyer-src
	elif [ -n "$have_network" ]; then
	    git clone -b release-${sawyer_version} "$sawyer_url" sawyer-src
	    (cd sawyer-src && git checkout -b r-${sawyer_version} release-${sawyer_version})
	    if [ -n "$downloads" ]; then
		ln -s sawyer-src download
		tar cf - download/. |gzip -9 >"$downloads/sawyer-0.0.0.tar.gz"
		rm download
	    fi
	else
	    echo "$arg0: cannot find sawyer-${sawyer_version}, and no network" >&2
	    eixt 1
	fi

        mkdir sawyer-bld
        cd sawyer-bld
        "$cmake_root/bin/cmake" ../sawyer-src \
              -DCMAKE_C_COMPILER="$c_exe" \
              -DCMAKE_CXX_COMPILER="$cxx_exe" \
              -DBOOST_ROOT="$boost_root" \
              -DCMAKE_INSTALL_PREFIX="$sawyer_root"
        make -j$ncpus install
    )
    rm -rf _build/sawyer-bld _build/sawyer-src
fi

#-------------------- Spock --------------------
(
    set -ex
    cd _build

    "$cmake_root/bin/cmake" .. \
          -DCMAKE_C_COMPILER="$c_exe" \
          -DCMAKE_CXX_COMPILER="$cxx_exe" \
          -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_MODULE_PATH=$(pwd)/../cmake \
          -DBOOST_ROOT="$boost_root" \
          -DSawyer_DIR="$sawyer_root/lib/cmake/Sawyer" \
          -DYamlCpp_ROOT="$yamlcpp_root" \
	  -DHOSTNAME="$SPOCK_HOSTNAME" \
          -DCMAKE_INSTALL_PREFIX="$prefix"

    make -j$ncpus
    make install
)

#-------------------- Initial setup --------------------
# make sure our own lib dirs are at the front in case the user has set this. Ideally, it shouldn't
# be necessary to set LD_LIBRARY_PATH at all, but unfortunately it is required in some situations.
export LD_LIBRARY_PATH="$boost_root/lib:$yamlcpp_root/lib:$sawyer_root/lib:$LD_LIBRARY_PATH"

export SPOCK_ROOT="$prefix"
export SPOCK_HOSTNAME
"$SPOCK_ROOT/bin/$SPOCK_HOSTNAME/spock-ls" --shellvars || exit 1
eval $($SPOCK_ROOT/bin/$SPOCK_HOSTNAME/spock-ls --export --shellvars)

echo
echo "Detecting system compilers"
echo "Note: errors during this step are usually harmless."
"$SPOCK_SCRIPTS/spock-install-system-compilers"

echo
echo "The following spock-managed software is installed:"
"$SPOCK_BINDIR/$SPOCK_HOSTNAME/spock-ls"

set +x

echo
echo "==== Spock has been installed ===="
echo
echo "Permanently adjust your shell environment, perhaps by editing ~/.bashrc:"
echo
echo "  * Add $SPOCK_ROOT/bin to your \$PATH."

if [ "$SPOCK_ROOT" != "$HOME/.spock" ]; then
    echo
    echo "  * Set the SPOCK_ROOT environment variable to"
    echo "    \"$SPOCK_ROOT\","
fi

echo
echo "  * Optionally set the SPOCK_BLDDIR to a fast, local filesystem. The"
echo "    default is \"/tmp\"."
echo
echo "  * If during the bootstrap you overrode any of the other variables listed in"
echo "    the output from \"spock-ls --shellvars\", then you should set those"
echo "    values permanently as well."
echo
echo "All spock and rmc commands support \"--help\"."
echo
echo "Thank you for using rmc-spock. Questions/comments/bugs can be addressed"
echo "to matzke@llnl.gov."
echo

exit 0
