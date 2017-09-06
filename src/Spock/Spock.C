#include <Spock/Spock.h>

#include <Spock/Context.h>
#include <Spock/DefinedPackage.h>
#include <Spock/GhostPackage.h>
#include <Spock/InstalledPackage.h>
#include <Spock/Solver.h>

#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <sstream>

using namespace Sawyer::Message::Common;

namespace Spock {

const char *VERSION = "2.1.2";
bool globalVerbose = false;                             // be extra verbose?
bool globalKeepTempFiles = false;                       // avoid deleting temporary files?

Sawyer::Message::PrefixPtr mprefix;
Sawyer::Message::DestinationPtr mdestination;


void
shutdown() {
    mprefix = Sawyer::Message::PrefixPtr();
    mdestination = Sawyer::Message::DestinationPtr();
}

void
initialize(Sawyer::Message::Facility &mlog) {
    static bool initialized = false;
    if (!initialized) {
        using namespace Sawyer::Message;

        Sawyer::initializeLibrary();

        if (!mprefix) {
            mprefix = Sawyer::Message::Prefix::instance();
            mprefix->showThreadId(false);
            mprefix->showElapsedTime(false);
            mprefix->showFacilityName(Sawyer::Message::Prefix::NEVER);
        }
        if (!mdestination)
            mdestination = Sawyer::Message::FileSink::instance(stderr)->prefix(mprefix);

        // Force certain facilities to be enabed or disabled. This might be different than the Sawyer default. If user wants
        // something else then use mfacilities.control("...") after we return. This doesn't affect any Facility object that's
        // already registered (such as any added already by the user or Sawyer's own, but we could use mfacilities.renable() if
        // we wanted that).
        mfacilities.impset(DEBUG, false);
        mfacilities.impset(TRACE, false);
        mfacilities.impset(WHERE, false);
        mfacilities.impset(MARCH, false);
        mfacilities.impset(INFO,  false);
        mfacilities.impset(WARN,  true);
        mfacilities.impset(ERROR, true);
        mfacilities.impset(FATAL, true);

        mlog = Facility("tool", mdestination);
        mfacilities.insertAndAdjust(mlog);

        Context::mlog = Facility("Spock::Context", mdestination);
        mfacilities.insertAndAdjust(Context::mlog);

        Solver::mlog = Facility("Spock::Solver", mdestination);
        mfacilities.insertAndAdjust(Solver::mlog);

        DefinedPackage::mlog = Facility("Spock::DefinedPackage", mdestination);
        mfacilities.insertAndAdjust(DefinedPackage::mlog);

        atexit(shutdown);
        initialized = true;
    }
}

Sawyer::CommandLine::Parser
commandLineParser(const std::string &purpose, const std::string &description, Sawyer::Message::Facility &mlog) {
    using namespace Sawyer::CommandLine;

    initialize(mlog);
    Parser p;
    p.errorStream(mlog[FATAL]);
    p.purpose(purpose);
    p.doc("Description", description);
    p.resetInclusionPrefixes();                         // because of "@12345678" being a hash

    SwitchGroup gen("General switches");

    gen.insert(Switch("help", 'h')
               .action(showHelpAndExit(0))
               .doc("Show this documentation."));

    gen.insert(Switch("version", 'V')
               .action(showVersionAndExit(Spock::VERSION, 0))
               .doc("Show the version number for this tool, then exit."));

    gen.insert(Switch("log")
               .action(configureDiagnostics("log", Sawyer::Message::mfacilities))
               .argument("config")
               .whichValue(SAVE_ALL)
               .doc("Configures diagnostics.  Use \"@s{log}=help\" and \"@s{log}=list\" to get started."));

    gen.insert(Switch("verbose", 'v')
               .intrinsicValue(true, globalVerbose)
               .doc("Make commands more verbose, mostly for debugging."));

    gen.insert(Switch("keep-temp")
               .intrinsicValue(true, globalKeepTempFiles)
               .doc("Keep temporary files, mostly for debugging."));

    p.with(gen);
    return p;
}

bool
isHash(const std::string &s) {
    if (s.size() != 8)
        return false;
    BOOST_FOREACH (char ch, s) {
        if (!isxdigit(ch))
            return false;
    }
    return true;
}

std::string
randomHash() {
    boost::random::random_device rng;
    boost::random::uniform_int_distribution<> word(0, 65535);
    char buf[16];
    sprintf(buf, "%04x%04x", word(rng), word(rng));
    return buf;
}

std::string
HashParser::docString() {
    return "A hash is eight hexadecimal characters.";
}

Sawyer::CommandLine::ParsedValue
HashParser::operator()(const char *input, const char **rest, const Sawyer::CommandLine::Location &loc) {
    if (strlen(input) < 8)
        throw std::runtime_error("hash expected");
    for (size_t i=0; i<8; ++i) {
        if (!isxdigit(input[i]))
            throw std::runtime_error("hash expected");
    }

    *rest = input + 8;
    std::string hash(input, *rest);
    ASSERT_require(isHash(hash));
    return Sawyer::CommandLine::ParsedValue(hash, loc, hash, valueSaver());
}

HashParser::Ptr
hashParser(std::string &storage) {
    return HashParser::instance(Sawyer::CommandLine::TypedSaver<std::string>::instance(storage));
}

HashParser::Ptr
hashParser(std::vector<std::string> &storage) {
    return HashParser::instance(Sawyer::CommandLine::TypedSaver<std::vector<std::string> >::instance(storage));
}

HashParser::Ptr
hashParser() {
    return HashParser::instance();
}

std::string
toString(const Aliases &aliases, bool terse) {
    std::ostringstream ss;
    if (aliases.isEmpty()) {
        if (!terse)
            ss <<"(empty set)";
    } else if (aliases.size() == 1) {
        ss <<aliases.least();
    } else if (aliases.size() == 2) {
        if (terse) {
            ss <<aliases.least() <<", " <<aliases.greatest();
        } else {
            ss <<aliases.least() <<" and " <<aliases.greatest();
        }
    } else {
        for (Aliases::ConstIterator value=aliases.values().begin(); value!=aliases.values().end(); ++value) {
            if (value != aliases.values().begin()) {
                ss <<", ";
                Aliases::ConstIterator next = value; ++next;
                if (next == aliases.values().end() && !terse)
                    ss <<"and ";
            }
            ss <<*value;
        }
    }
    return ss.str();
}

InstalledPackage::Ptr
asInstalled(const Package::Ptr &pkg) {
    InstalledPackage::Ptr installed = pkg.dynamicCast<InstalledPackage>();
    ASSERT_always_require(pkg==NULL || installed!=NULL);
    return installed;
}

GhostPackage::Ptr
asGhost(const Package::Ptr &pkg) {
    GhostPackage::Ptr ghost = pkg.dynamicCast<GhostPackage>();
    ASSERT_always_require(pkg==NULL || ghost!=NULL);
    return ghost;
}

} // namespace
