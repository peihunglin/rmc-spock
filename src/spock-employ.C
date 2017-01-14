static const char *purpose = "cause an installed package to be available for use";
static const char *description =
    "Given an installation hash, run a command in a subshell where that installed package and its runtime dependencies have "
    "been added to the environment.  If no command is specified then run an interactive subshell.";

#include <Spock/Context.h>

using namespace Spock;
using namespace Sawyer::Message::Common;

namespace {

Sawyer::Message::Facility mlog;

struct Settings {
    std::vector<std::string> hashes;                    // package installations
};

std::vector<std::string>
parseCommandLine(int argc, char *argv[], Settings &settings) {
    using namespace Sawyer::CommandLine;

    Parser p = commandLineParser(purpose, description, mlog);
    p.doc("Synopsis", "@prop{programName} @s{package}=@v{hash} [@v{command}...]");

    SwitchGroup tool("Tool-specific switches");

    tool.insert(Switch("package", 'p')
                .argument("hash", listParser(Spock::hashParser(settings.hashes)))
                .whichValue(SAVE_ALL)
                .explosiveLists(true)
                .doc("Specifies the hashes of installed packages that should be employed in the subshell. The hashes can "
                     "be separated by commas or colons, and/or this switch may appear more than once on the command-line. If "
                     "no hashes are specified then the subshell uses the same environment as its parent."));

    return p.with(tool).parse(argc, argv).apply().unreachedArgs();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace


int
main(int argc, char *argv[]) {
    Spock::initialize(mlog);
    Settings settings;
    std::vector<std::string> command = parseCommandLine(argc, argv, settings);

    Spock::Context ctx;
    bool changed = false;
    BOOST_FOREACH (const std::string &hash, settings.hashes) {
        if (ctx.employ(hash))
            changed = true;
    }
    if (!changed)
        mlog[WARN] <<"no changes made to environment\n";

    return ctx.subshell(command);
}
