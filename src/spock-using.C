static const char *purpose = "list packages that are in use";
static const char *description =
    "Lists the names of packages that are currently in use. If arguments are supplied, they should be package "
    "patterns that will filter the output so it includes only those packages that match.";

#include <Spock/Context.h>
#include <Spock/Exception.h>
#include <Spock/Package.h>
#include <Spock/PackagePattern.h>

using namespace Spock;
using namespace Sawyer::Message::Common;

namespace {

bool machineOutput = false;
Sawyer::Message::Facility mlog;

std::vector<std::string>
parseCommandLine(int argc, char *argv[]) {
    using namespace Sawyer::CommandLine;

    Parser p = commandLineParser(purpose, description, mlog);

    p.with(Switch("fields")
           .intrinsicValue(true, machineOutput)
           .doc("The output consists of three columns separated by tab characters: the base name of the package, the "
                "version number, and the hash."));
    p.with(Switch("no-fields")
           .key("fields")
           .intrinsicValue(false, machineOutput)
           .hidden(true));
    
    p.doc("Synopsis", "@prop{programName} [@v{patterns}]");
    return p.parse(argc, argv).apply().unreachedArgs();
}

bool
shouldShow(const Package::Ptr &pkg, const std::vector<PackagePattern> &patterns) {
    ASSERT_not_null(pkg);
    if (patterns.empty())
        return true;
    BOOST_FOREACH (const PackagePattern &pattern, patterns) {
        if (pattern.matches(pkg))
            return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace

int
main(int argc, char *argv[]) {
    Spock::initialize(mlog);
    std::vector<std::string> args = parseCommandLine(argc, argv);
    Spock::Context ctx;

    std::vector<PackagePattern> patterns;
    BOOST_FOREACH (const std::string &arg, args)
        patterns.push_back(PackagePattern(arg));
    
    try {
        BOOST_FOREACH (const Package::Ptr &pkg, ctx.employed()) {
            if (shouldShow(pkg, patterns)) {
                if (machineOutput) {
                    std::cout <<pkg->name() <<"\t" <<pkg->version().toString() <<"\t" <<pkg->hash() <<"\n";
                } else {
                    std::cout <<pkg->toString() <<"\n";
                }
            }
        }
    } catch (const Exception::SpockError &e) {
        mlog[ERROR] <<e.what() <<"\n";
        exit(1);
    }
}
