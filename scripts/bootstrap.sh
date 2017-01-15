#!/bin/bash
# This script tries to build Spock and its dependencies.
# You should invoke this script in the top of the Spock
# source tree as "./scripts/bootstrap.sh".
set -e
arg0="${0##*/}"


#-------------------- Commandline parsing --------------------

prefix="$HOME/.spock"
downloads=
while [ "$#" -gt 0 ]; do
    case "$1" in
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

	# Where to install Spock's files.
	--prefix=*)
	    prefix="${1#--prefix=}"
	    shift
	    ;;
	--prefix)
	    prefix="$2"
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
	    echo "usage: $0 [--prefix=DIRECTORY]" >&2
	    exit 1
	    ;;
    esac
done
[ "$downloads" = "" ] && downloads="$prefix/var/downloads"
mkdir -p "$downloads" || exit 1

#-------------------- Basic Setup --------------------
if [ ! -e src/Spock/Spock.h -o ! -e scripts/bootstrap.sh ]; then
    echo "$arg0: error: invoke this script from the top of the Spock source tree" >&2
    exit 1
fi
if [ "$(whoami)" = "root" ]; then
    echo "$arg0: error: do not compile programs as root" >&2
    exit 1
fi
if ! mkdir _build; then
    echo "$arg0: please delete your old '_build' directory first" >&2
    exit 1
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
cxx_version=$(echo "$cxx_quad" |cut -d: -f4)


# How many threads can we use for compiling?
ncpus=$(sed -n '/^processor[ \t]\+:/p' </proc/cpuinfo |wc -l)
[ "$ncpus" = "" -o "$ncpus" -eq 0 ] && ncpus=1


#-------------------- Boost --------------------
: ${boost_url:=http://sourceforge.net/projects/boost/files/boost/1.62.0/boost_1_62_0.tar.bz2/download}
boost_libs=chrono,date_time,filesystem,iostreams,program_options,random,regex,serialization,signals,system,thread,wave

if [ ! -d "$prefix/dependencies/boost" ]; then
    (
        set -ex
	mkdir -p _build/boost
        cd _build/boost
        if [ -e "$downloads/boost-1.62.0.tar.gz" ]; then
            tar xf "$downloads/boost-1.62.0.tar.gz"
            mv download boost_1_62_0
	elif [ -e "$downloads/boost_1_62_0.tar.bz2" ]; then
	    tar xf "$downloads/boost_1_62_0.tar.bz2"
        else
            wget -O - "$boost_url" |tar xjf -
        fi
	if [ -n "$downloads" -a ! -e "$downloads/boost-1.62.0.tar.gz" ]; then
	    ln -s boost_1_62_0 download
	    tar cf - download/. |gzip -9 >"$downloads/boost-1.62.0.tar.gz"
	    rm download
	fi
        cd boost_1_62_0

	boost_cxx_vendor=
	case "$cxx_vendor" in
	    gnu) boost_cxx_vendor=gcc ;;
	    llvm) boost_cxx_vendor=clang ;;
	    *) boost_cxx_vendor="$cxx_vendor" ;;
	esac
	       
        echo "using $boost_cxx_vendor : $cxx_version : $cxx_exe ;" >tools/build/src/user-config.jam
        ./bootstrap.sh --prefix="$prefix/dependencies/boost" --with-libraries="$boost_libs" --toolset="$boost_cxx_vendor"
        ./b2 --prefix="$prefix/dependencies/boost" -sNO_BZIP2=1 toolset="$boost_cxx_vendor"
        ./b2 --prefix="$prefix/dependencies/boost" -sNO_BZIP2=1 toolset="$boost_cxx_vendor" install
    )
    rm -rf _build/boost_1_62_0
fi

#-------------------- Yaml-cpp --------------------
: ${yamlcpp_url:=https://github.com/jbeder/yaml-cpp}

if [ ! -d "$prefix/dependencies/yamlcpp" ]; then
    (
        set -ex
	cd _build

	if [ -e "$downloads/yamlcpp-0.5.3.tar.gz" ]; then
	    tar xf "$downloads/yamlcpp-0.5.3.tar.gz"
	    mv download yamlcpp-src
	else
            git clone -b release-0.5.3 "$yamlcpp_url" yamlcpp-src
            (cd yamlcpp-src && git checkout -b r053 release-0.5.3)
	    if [ -n "$downloads" ]; then
		ln -s yamlcpp-src download
		tar cf - download/. |gzip -9 >"$downloads/yamlcpp-0.5.3.tar.gz"
		rm download
	    fi
	fi

        mkdir yamlcpp-bld
        cd yamlcpp-bld
        cmake ../yamlcpp-src \
              -DCMAKE_C_COMPILER="$c_exe" \
              -DCMAKE_CXX_COMPILER="$cxx_exe" \
              -DBUILD_SHARED_LIBS=Yes \
              -DBOOST_ROOT="$prefix/dependencies/boost" \
              -DCMAKE_INSTALL_PREFIX="$prefix/dependencies/yamlcpp"
        make -j$ncpus install
    )
    rm -rf _build/yamlcpp-bld _build/yamlcpp-src
fi

#-------------------- Sawyer --------------------
: ${sawyer_url:=https://github.com/matzke1/sawyer}

if [ ! -d "$prefix/dependencies/sawyer" ]; then
    (
        set -ex
        cd _build

	if [ -e "$downloads/sawyer-0.0.0.tar.gz" ]; then
	    tar xf "$downloads/sawyer-0.0.0.tar.gz"
	    mv download sawyer-src
	else
	    git clone "$sawyer_url" sawyer-src
	    if [ -n "$downloads" ]; then
		ln -s sawyer-src download
		tar cf - download/. |gzip -9 >"$downloads/sawyer-0.0.0.tar.gz"
		rm download
	    fi
	fi

        mkdir sawyer-bld
        cd sawyer-bld
        cmake ../sawyer-src \
              -DCMAKE_C_COMPILER="$c_exe" \
              -DCMAKE_CXX_COMPILER="$cxx_exe" \
              -DBOOST_ROOT="$prefix/dependencies/boost" \
              -DCMAKE_INSTALL_PREFIX="$prefix/dependencies/sawyer"
        make -j$ncpus install
    )
    rm -rf _build/sawyer-bld _build/sawyer-src
fi

#-------------------- Spock --------------------
(
    set -ex
    cd _build

    cmake .. \
          -DCMAKE_C_COMPILER="$c_exe" \
          -DCMAKE_CXX_COMPILER="$cxx_exe" \
          -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_MODULE_PATH=$(pwd)/../cmake \
          -DBOOST_ROOT="$prefix/dependencies/boost" \
          -DSawyer_DIR="$prefix/dependencies/sawyer/lib/cmake/Sawyer" \
          -DYamlCpp_ROOT="$prefix/dependencies/yamlcpp" \
          -DCMAKE_INSTALL_PREFIX="$prefix"

    make -j$ncpus
    make install
)

#-------------------- Initial setup --------------------
export SPOCK_ROOT="$prefix"
"$SPOCK_ROOT/bin/spock-ls" --shellvars || exit 1
eval $($prefix/bin/spock-ls --export --shellvars)
"$SPOCK_SCRIPTS/spock-install-system-compilers"
"$SPOCK_BINDIR/spock-ls"

set +x

echo
echo "==== Spock has been installed ===="
echo
echo "Now be sure to make these changes to your environment. You can"
echo "make these permanent by copying these commands to $HOME/.bashrc"
echo
echo "export PATH=\"$SPOCK_BINDIR:\$PATH\""
echo "export SPOCK_ROOT=\"$SPOCK_ROOT\" # A large filesystem"
echo "export SPOCK_BLDDIR=/tmp # A fast filesystem"
echo
echo "You can check other directories by running \"spock-ls --shellvars\""
