static const char *purpose = "list installed packages";
static const char *description =
    "Reads the installed package database and lists those packages that match the patterns specified on the command-line. If "
    "no patterns are specified then all installed packages are listed.";

#include <Spock/Context.h>
#include <Spock/Exception.h>
#include <Spock/Package.h>
#include <Spock/PackagePattern.h>

using namespace Spock;
using namespace Sawyer::Message::Common;

namespace {

Sawyer::Message::Facility mlog;
bool listSelf = false;                                  // list spock's own specification string
bool listShellVariables = false;                        // list shell variable settings
bool exportVars = false;                                // if listShellVariables, prefix them each with "export "
bool showDeps = true;                                   // show direct dependencies?
bool showComments = false;                              // adds comments in parentheses
bool findingGhosts = false;                             // find installable packages rather than installed packages?
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
        std::cout <<expt <<"SPOCK_ROOT='" <<ctx.rootDirectory().string() <<"'\n";
        std::cout <<expt <<"SPOCK_BINDIR='" <<ctx.binDirectory().string() <<"'\n";
        std::cout <<expt <<"SPOCK_OPTDIR='" <<ctx.optDirectory().string() <<"'\n";
        std::cout <<expt <<"SPOCK_PKGDIR='" <<ctx.packageDirectory().string() <<"'\n";
        std::cout <<expt <<"SPOCK_VARDIR='" <<ctx.varDirectory().string() <<"'\n";
        std::cout <<expt <<"SPOCK_SCRIPTS='" <<ctx.scriptDirectory().string() <<"'\n";
        std::cout <<expt <<"SPOCK_BLDDIR='" <<ctx.buildDirectory().string() <<"'\n";
        std::cout <<expt <<"SPOCK_VERSION='" <<VERSION <<"'\n";
        std::cout <<expt <<"SPOCK_SPEC='" <<ctx.spockItself()->toString() <<"'\n";
        exit(0);
    }
    
    bool hadError = false;
    try {
        std::vector<Package::Ptr> packages = findByPatterns(ctx, patterns);

        if (!showGraph.empty()) {
            std::ofstream gv(showGraph.string().c_str());
            gv <<ctx.toGraphViz(ctx.dependencyLattice(packages));
        }

        BOOST_FOREACH (const Package::Ptr &pkg, packages) {
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
