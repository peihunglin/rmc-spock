static const char *purpose = "list installed packages";
static const char *description =
    "Reads the installed package database and lists those packages that match the patterns specified on the command-line. If "
    "no patterns are specified then all installed packages are listed.";

#include <Spock/Context.h>
#include <Spock/Exception.h>
#include <Spock/InstalledPackage.h>
#include <Spock/Package.h>
#include <Spock/PackagePattern.h>
#include <Spock/Solver.h>
#include <boost/date_time/posix_time/time_formatters.hpp>

using namespace Spock;
using namespace Sawyer::Message::Common;

namespace {

Sawyer::Message::Facility mlog;
bool listSelf = false;                                  // list spock's own specification string
bool listShellVariables = false;                        // list shell variable settings
bool exportVars = false;                                // if listShellVariables, prefix them each with "export "
bool showDeps = true;                                   // show direct dependencies?
bool showComments = false;                              // adds comments in parentheses
bool showUsedTime = false;                              // show time of last use
bool findingGhosts = false;                             // find installable packages rather than installed packages?
bool excludeUnusable = false;                           // exclude installed packages that can't be used in current environment
boost::filesystem::path showGraph;                      // generate a dependency graph

std::vector<std::string>
parseCommandLine(int argc, char *argv[]) {
    using namespace Sawyer::CommandLine;
    Parser p = commandLineParser(purpose, description, mlog);
    p.doc("Synopsis", "@prop{programName} [@v{patterns}]");

    p.with(Switch("self")
           .intrinsicValue(true, listSelf)
           .doc("Show the specification for this version of spock instead of doing anything else."));

    p.with(Switch("shellvars")
           .intrinsicValue(true, listShellVariables)
           .doc("Emit shell commands that set various Spock environment variables if they're not already set."));

    p.with(Switch("export")
           .intrinsicValue(true, exportVars)
           .doc("When the @s{shellvars} switch is used, make them exports."));

    p.with(Switch("ghosts")
           .intrinsicValue(true, findingGhosts)
           .doc("List ghost (installable) packages rather than installed packages."));

    p.with(Switch("top", '1')
           .intrinsicValue(false, showDeps)
           .doc("If present, show each top-level spec, otherwise show the top-level spec and each of its dependencies."));

    p.with(Switch("comments", 'v')
           .intrinsicValue(true, showComments)
           .doc("Show comments about some packages."));

    p.with(Switch("graph")
           .argument("file", anyParser(showGraph))
           .doc("Generate a GraphViz dependency graph and save it in the specified @v{file}."));

    p.with(Switch("used")
           .intrinsicValue(true, showUsedTime)
           .doc("Sort installed packages by the time they were last used by spock-shell, and emit this time in the listing."));

    p.with(Switch("usable")
           .intrinsicValue(true, excludeUnusable)
           .doc("When listing installed packages, exclude those which cannot be used due to the environment already having "
                "conflicting packages. For instance, to get a list of only those compilers that run in m32 mode you can "
                "bring into service the m32-generator (via \"spock-shell --with m32-generator\") and then restrict the "
                "compiler listing to those that don't conflict with m32-generator (e.g., \"@prop{programName} --usable "
                "c++-compiler\")."));

    p.doc("Compiler Names",
          "The following compiler names are generally available:"

          "@named{@v{vendor}-@v{language}[@v{version-spec}]}"
          "{These are the actual compilers and are what you should normally use. The @v{vendor} is one of \"gnu\", \"llvm\", "
          "or \"intel\".  The @v{language} is usually the name of the standard or pseudo-standard that defines the language "
          "such as \"c89\", \"gnu89\", \"c99\", \"gnu99\", \"c11\", \"gnu11\" for C compilers; \"c++03\", \"gnu++03\", \"c++11\", "
          "\"gnu++11\", \"c++14\", \"gnu++14\" for C++ compilers; \"fortran\" for Fortran compilers (we don't support various "
          "Fortran language standards yet).}"

          "@named{@v{language}-compiler[@v{version-spec}]}"
          "{These are virtual packages which serve as sets of compilers for a specific language. The @v{language} is the same "
          "as the previous bullet. For instance, \"c++11-compiler\" is a virtual package to which all C++11 compilers belong.}"

          "@named{@v{baselang}-compiler[@v{version-spec}]}"
          "{These are virtual package which serve as sets of compilers. The @v{baselang} is the basic language rather than a "
          "standard, such as \"c\", \"c++\", or \"fortran\".  Base languages are ambiguous; e.g., if you ask for \"c++-compiler\" "
          "you're going to get any C++ language that satisfies all other requirements that you might have specified.}"

          "@named{default-@v{baselang}[@v{version-spec}]}"
          "{Within a collection of compilers, say GCC-6.3, there is an executable that represents the C++ compilers and can "
          "handle C++89, C++11, C++14, and GNU variants thereof. If you execute this compiler with no arguments it will parse "
          "a particular C++ language. The particular default language various from version to version and vendor to vendor. "
          "For instance, \"default-c++\" for the GCC-4 collections is GNU++03, while the default C++ for GCC-6 collections is "
          "GNU++14. Therefore, these virtual \"default-@v{baselang}\" packages are aliases for whatever is the default language "
          "standard.}"

          "@named{@v{vendor}-compilers[@v{version-spec}]}"
          "{These are the compiler collections (thus the plural \"compilers\"). These packages don't actually cause any "
          "particular compiler to be used, rather they serve as a container for a bunch of compilers for usually a quite "
          "large number of languages.  For instance, \"gnu-compilers-6.3.0\" is the container that holds all the GCC-6.3.0 "
          "compilers, but if you say \"spock-shell --with gnu-compilers-6.3.0\" your environment won't actually contain any "
          "specific compilers -- you also need to select a compiler using one of the packages documented above. Furthermore, "
          "@v{vendor}-system-compilers is similar, but represents compilers that are already installed in your operating "
          "system rather than compilers that spock installed.}"

          "Here are some examples:"
          "@named{gnu-c++11-6.3}{Any GNU C++11 compiler from any GCC 6.3.x version. This might include those compilers that "
          "generate 32-bit code on a 64-bit platform (although spock-shell will prefer to use those that generate 64-bit code). "
          "Within a spock-shell environment, C++ compiler executables are always named \"c++\", C compilers are always named "
          "\"cc\", and fortran compilers are always named \"fc\". There might be other names in $PATH for these compilers too, "
          "but don't depend on that.}"

          "@named{intel-compilers-17,default-c++}{This pair of constraints means that \"c++\" in the spock-shell environment "
          "should invoke a compiler for whatever language is the default for the Intel version 17.x.y compiler collection.}"

          "@named{m32-generation,c89-compiler}{This pair of constraints will cause \"cc\" to be a C89 compiler that "
          "generates 32-bit code even on a 64-bit platform. It does not constrain the vendor or compiler version. "
          "In order for m32-generation compilers to be available, your system generally needs \"multilib\" support "
          "appropriate for particular compiler versions.}");

    p.doc("System Compilers",
          "If your operating system has compilers already installed, then spock can find and use them. In fact, you need "
          "to have at least one set of compilers installed because you need a C++ compiler to compile spock itself. When "
          "spock is installed with the \"./scripts/bootstrap.sh --upgrade\" script it will search for the system-installed "
          "compilers and add them to spock's package list.  If a system compiler is upgraded, then spock will refuse to "
          "run the new version even if it has the same command name as the old version. In order to find new system "
          "compilers, run \"$SPOCK_ROOT/scripts/spock-install-system-compilers\" with no arguments. In order to delete "
          "a compiler or a compiler collection (names are defined in the \"Compiler Names\" section) use the \"spock-rm\" "
          "tool, which will also delete all package that depend on the compiler(s) being deleted.");

    return p.parse(argc, argv).apply().unreachedArgs();
}

bool
sortByName(const Package::Ptr &a, const Package::Ptr &b) {
    return a->toString() < b->toString();
}

bool
sameName(const Package::Ptr &a, const Package::Ptr &b) {
    return a->toString() == b->toString();
}

bool
sortByLastUsed(const Package::Ptr &a, const Package::Ptr &b) {
    if (a->isInstalled() && b->isInstalled()) {
        boost::posix_time::ptime aLastUse = asInstalled(a)->usedTimeStamp();
        boost::posix_time::ptime bLastUse = asInstalled(b)->usedTimeStamp();
        return aLastUse < bLastUse;
    } else if (a->isInstalled() != b->isInstalled()) {
        return a->isInstalled();
    } else {
        return false;
    }
}

Packages
findByPattern(const Context &ctx, const PackagePattern &pattern) {
    if (findingGhosts)
        return ctx.findGhosts(pattern);
    return ctx.findInstalled(pattern);
}

Packages
findByPatterns(const Context &ctx, const std::vector<std::string> &patterns) {
    Packages retval;
    if (patterns.empty()) {
        retval = findByPattern(ctx, PackagePattern());
    } else {
        BOOST_FOREACH (const std::string &pattern, patterns) {
            Packages pkgs = findByPattern(ctx, pattern);
            if (pkgs.empty()) {
                mlog[WARN] <<"no package matching \"" <<pattern <<"\"\n";
            } else {
                retval.insert(retval.end(), pkgs.begin(), pkgs.end());
            }
        }
    }
    std::sort(retval.begin(), retval.end(), sortByName);
    retval.erase(std::unique(retval.begin(), retval.end(), sameName), retval.end());
    return retval;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace

int
main(int argc, char *argv[]) {
    Spock::initialize(mlog);
    std::vector<std::string> patterns = parseCommandLine(argc, argv);

    Spock::Context ctx;
    if (listSelf) {
        std::cout <<ctx.spockItself()->toString() <<"\n";
        exit(0);
    }

    if (listShellVariables) {
        std::string expt = exportVars ? "export " : "";
        std::cout <<expt <<"SPOCK_VERSION='" <<VERSION <<"'\n";
        std::cout <<expt <<"SPOCK_SPEC='" <<ctx.spockItself()->toString() <<"'\n";
        std::cout <<expt <<"SPOCK_HOSTNAME='" <<ctx.hostName() <<"'\n";
        std::cout <<expt <<"SPOCK_ROOT='" <<ctx.rootDirectory().string() <<"'\n";
        std::cout <<expt <<"SPOCK_BINDIR='" <<ctx.binDirectory().string() <<"'\n";
        std::cout <<expt <<"SPOCK_OPTDIR='" <<ctx.optDirectory().string() <<"'\n";
        std::cout <<expt <<"SPOCK_PKGDIR='" <<ctx.packageDirectory().string() <<"'\n";
        std::cout <<expt <<"SPOCK_VARDIR='" <<ctx.varDirectory().string() <<"'\n";
        std::cout <<expt <<"SPOCK_SCRIPTS='" <<ctx.scriptDirectory().string() <<"'\n";
        std::cout <<expt <<"SPOCK_BLDDIR='" <<ctx.buildDirectory().string() <<"'\n";
        exit(0);
    }
    
    bool hadError = false;
    try {
        std::vector<Package::Ptr> packages = findByPatterns(ctx, patterns);

        if (!showGraph.empty()) {
            std::ofstream gv(showGraph.string().c_str());
            gv <<ctx.toGraphViz(ctx.dependencyLattice(packages));
        }

        if (showUsedTime)
            std::sort(packages.begin(), packages.end(), sortByLastUsed);

        BOOST_FOREACH (const Package::Ptr &pkg, packages) {
            if (excludeUnusable) {
                Solver solver(ctx);
                if (solver.solve(pkg->toString()) == 0)
                    continue;
            }
            
            std::cout <<pkg->toString();

            if (showComments) {
                if (pkg->isInstalled()) {
                    if (!pkg->aliases().isEmpty())
                        std::cout <<"(" <<toString(pkg->aliases(), true /*terse*/) <<")";
                } else {
                    std::cout <<"(";
                    VersionNumbers vnums = pkg->versions();
                    BOOST_FOREACH (const VersionNumber &v, vnums.values()) {
                        if (v != *vnums.values().begin())
                            std::cout <<", ";
                        std::cout <<v.toString();
                    }
                    std::cout <<")";
                }
            }

            if (showUsedTime && pkg->isInstalled())
                std::cout <<" " <<boost::posix_time::to_simple_string(asInstalled(pkg)->usedTimeStamp());
            
            if (showDeps) {
                BOOST_FOREACH (const PackagePattern &deppat, pkg->dependencyPatterns())
                    std::cout <<" " <<deppat.toString();
            }
            std::cout <<"\n";
        }
    } catch (const Exception::SpockError &e) {
        mlog[ERROR] <<e.what() <<"\n";
        hadError = true;
    }
    return hadError ? 1 : 0;
}
