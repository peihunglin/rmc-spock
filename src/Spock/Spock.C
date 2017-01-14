#include <Spock/Spock.h>

using namespace Sawyer::Message::Common;

namespace Spock {

const std::string VERSION = "0.0.1";

Sawyer::Message::PrefixPtr mprefix;
Sawyer::Message::DestinationPtr mdestination;

void
initialize(Sawyer::Message::Facility &mlog) {
    Sawyer::initializeLibrary();

    if (!mprefix)
        mprefix = Sawyer::Message::Prefix::instance();
    if (!mdestination)
        mdestination = Sawyer::Message::FileSink::instance(stderr)->prefix(mprefix);
    mlog = Sawyer::Message::Facility("tool", mdestination);
}

Sawyer::CommandLine::Parser
commandLineParser(const std::string &purpose, const std::string &description, Sawyer::Message::Facility &mlog) {
    using namespace Sawyer::CommandLine;

    initialize(mlog);
    Parser p;
    p.errorStream(mlog[FATAL]);
    p.purpose(purpose);
    p.doc("Description", "description");

    SwitchGroup gen("General switches");

    gen.insert(Switch("help", 'h')
               .action(showHelpAndExit(0))
               .doc("Show this documentation."));

    gen.insert(Switch("version", 'V')
               .action(showVersionAndExit(Spock::VERSION, 0))
               .doc("Show the version number for this tool, then exit."));

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


} // namespace
