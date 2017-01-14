#ifndef Spock_Spock_H
#define Spock_Spock_H

// Common things needed by our public header files
#include <boost/foreach.hpp>
#include <Sawyer/CommandLine.h>
#include <Sawyer/Map.h>
#include <Sawyer/Message.h>
#include <Sawyer/Sawyer.h>
#include <Sawyer/SharedPointer.h>
#include <string>
#include <vector>

namespace Spock {

extern const std::string VERSION;

/** To store PATH-like environment variables. */
typedef Sawyer::Container::Map<std::string, std::vector<std::string> > SearchPaths;

// Pointer types for forward class declarations
typedef Sawyer::SharedPointer<class InstalledPackage> InstalledPackagePtr;
typedef Sawyer::SharedPointer<class GlobalFlag> GlobalFlagPtr;

/** Initialize this library.
 *
 *  This function must be called before any other of this library's functions. */
void initialize(Sawyer::Message::Facility &mlog);

/** Command-line parsing. */
Sawyer::CommandLine::Parser commandLineParser(const std::string &purpose, const std::string &description,
                                              Sawyer::Message::Facility &mlog);


/** Escape special characters like in C strings. */
std::string cEscape(const std::string&);

/** Determine whether a string is a valid hash. */
bool isHash(const std::string&);

class HashParser: public Sawyer::CommandLine::ValueParser {
protected:
    HashParser() {}
    HashParser(const Sawyer::CommandLine::ValueSaver::Ptr &valueSaver)
        : Sawyer::CommandLine::ValueParser(valueSaver) {}

public:
    typedef Sawyer::SharedPointer<HashParser> Ptr;

    static Ptr instance() {
        return Ptr(new HashParser);
    }

    static Ptr instance(const Sawyer::CommandLine::ValueSaver::Ptr &valueSaver) {
        return Ptr(new HashParser(valueSaver));
    }

    static std::string docString();

private:
    virtual Sawyer::CommandLine::ParsedValue operator()(const char *input, const char **rest,
                                                        const Sawyer::CommandLine::Location &loc);
};

HashParser::Ptr hashParser(std::string &storage);
HashParser::Ptr hashParser(std::vector<std::string> &storage);
HashParser::Ptr hashParser();

} // namespace

#endif
