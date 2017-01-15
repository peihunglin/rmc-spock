static const char *purpose = "remove packages";
static const char *description =
    "Removes all trace of specified installed packages and, recursively, those installed packages that depend on them.";

#include <Spock/Context.h>
#include <Spock/Exception.h>
#include <Spock/InstalledPackage.h>
#include <Spock/Package.h>
#include <Spock/PackagePattern.h>

#include <Sawyer/GraphTraversal.h>

using namespace Spock;
using namespace Sawyer::Message::Common;

namespace {

Sawyer::Message::Facility mlog;
bool dryRun = false;
bool useForce = false;

std::vector<std::string>
parseCommandLine(int argc, char *argv[]) {
    using namespace Sawyer::CommandLine;
    Parser p = commandLineParser(purpose, description, mlog);
    p.doc("Synopsis", "@prop{programName} [@v{patterns}]");

    p.with(Switch("dry-run", 'n')
           .intrinsicValue(true, dryRun)
           .doc("Print what would be removed, but do not remove it."));

    p.with(Switch("force", 'f')
           .intrinsicValue(true, useForce)
           .doc("Forcibly do the operation."));
    
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace

int
main(int argc, char *argv[]) {
    Spock::initialize(mlog);
    mlog[INFO].enable();                                // progress reports since removal can take a long time
    std::vector<std::string> patterns = parseCommandLine(argc, argv);

    Context ctx;
    
    bool hadError = false;
    try {

        // Find all matching packages
        std::vector<Package::Ptr> packages;
        if (patterns.empty()) {
            if (!useForce) {
                mlog[FATAL] <<"refusing to remove all installed packages; use --force to override\n";
                exit(1);
            }
            packages = ctx.findInstalled(PackagePattern());
        } else {
            BOOST_FOREACH (const std::string &pattern, patterns) {
                Packages found = ctx.findInstalled(pattern);
                if (found.empty()) {
                    mlog[WARN] <<"no package matching \"" <<pattern <<"\"\n";
                } else {
                    packages.insert(packages.end(), found.begin(), found.end());
                }
            }
        }
        std::sort(packages.begin(), packages.end(), sortByName);
        packages.erase(std::unique(packages.begin(), packages.end(), sameName), packages.end());

        // Dependency lattice containing all installed packages
        Context::Lattice lattice = ctx.dependencyLattice(ctx.findInstalled(PackagePattern()));

        // For each package to remove, do a depth first search of the dependency lattice following incoming edges to find all
        // packages that depend on each package.
        std::vector<Package::Ptr> toRemove;
        BOOST_FOREACH (const Package::Ptr &pkg, packages) {
            Context::Lattice::VertexIterator start = lattice.findVertexKey(pkg->toString());
            ASSERT_require(lattice.isValidVertex(start));
            typedef Sawyer::Container::Algorithm::DepthFirstReverseVertexTraversal<Context::Lattice> Traversal;
            for (Traversal t(lattice, start); t; ++t) {
                std::string spec = t->value();
                std::vector<Package::Ptr> found = ctx.findInstalled(spec);
                ASSERT_require(found.size()==1);
                toRemove.push_back(found[0]);
            }
        }

        // Remove packages in the reverse order they were found so that if the user interrupts this processes the system is
        // still in a valid state.
        std::set<std::string> removed;
        BOOST_REVERSE_FOREACH (const Package::Ptr &pkg, toRemove) {
            if (removed.insert(pkg->toString()).second) {
                if (dryRun) {
                    std::cout <<pkg->toString() <<"\n";
                } else {
                    mlog[INFO] <<"removing " <<pkg->toString() <<"\n";
                    asInstalled(pkg)->remove(ctx);
                    ctx.deregister(pkg);
                }
            }
        }

    } catch (const Exception::SpockError &e) {
        mlog[ERROR] <<e.what() <<"\n";
        hadError = true;
    }
    return hadError ? 1 : 0;
}
