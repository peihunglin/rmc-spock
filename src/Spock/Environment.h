#ifndef Spock_Environment_H
#define Spock_Environment_H

#include <Spock/Spock.h>

namespace Spock {

/** Manages environment variables. */
class Environment {
    typedef Sawyer::Container::Map<std::string /*name*/, std::string /*value*/> Map;
    Map variables_;

public:
    /** Empty environment. */
    Environment() {}

    /** Initialize from process environment. */
    void reload();

    /** Set an environment variable. */
    void set(const std::string &variable, const std::string &value);

    /** Append a value to an existing variable. */
    void append(const std::string &variable, const std::string &value, const std::string &separator = ":");

    /** Prepend a value to an existing variable. */
    void prepend(const std::string &variable, const std::string &value, const std::string &separator = ":");

    /** Prepend all the variables in other to the variables in this. */
    void prepend(const Environment &other);

    /** Export environment variables to the environment.
     *
     *  This completely rewrites the process environment. Any variables that were there before will be erased and replaced with
     *  variables only from this object. */
    void exportVars() const;
};

} // namespace

#endif
