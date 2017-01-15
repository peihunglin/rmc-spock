static const char *purpose = "list packages that are in use";
static const char *description =
    "Lists the names of packages that are currently in use.";

#include <Spock/Context.h>
#include <Spock/Exception.h>
#include <Spock/Package.h>

using namespace Spock;
using namespace Sawyer::Message::Common;

namespace {

Sawyer::Message::Facility mlog;

std::vector<std::string>
parseCommandLine(int argc, char *argv[]) {
    using namespace Sawyer::CommandLine;
    Parser p = commandLineParser(purpose, description, mlog);
    p.doc("Synopsis", "@prop{programName} [@v{patterns}]");
    return p.parse(argc, argv).apply().unreachedArgs();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace

int
main(int argc, char *argv[]) {
    Spock::initialize(mlog);
    std::vector<std::string> patterns = parseCommandLine(argc, argv);
    Spock::Context ctx;

    try {
        BOOST_FOREACH (const Package::Ptr &pkg, ctx.employed())
            std::cout <<pkg->toString() <<"\n";
    } catch (const Exception::SpockError &e) {
        mlog[ERROR] <<e.what() <<"\n";
        exit(1);
    }
}
