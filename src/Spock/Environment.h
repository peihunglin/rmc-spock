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

    /** Get an environment variable.
     *
     *  If the variable is not defined, then return the default value without assigning it to the variable. */
    std::string get(const std::string &variable, const std::string &dflt = "") const;

    /** Append a value to an existing variable.
     *
     *  The variable and new value are assumed to be parts separated by a separator. The parts of the value are
     *  each appended to the variable if the part doesn't already exist in the variable. */
    void appendUnique(const std::string &variable, const std::string &value, const std::string &separator = ":");

    /** Prepend a value to an existing variable.
     *
     *  The variable and new value are assumed to be parts separated by a separator. The parts of the value are
     *  each prepended to the variable if the part doesn't already exist in the variable. */
    void prependUnique(const std::string &variable, const std::string &value, const std::string &separator = ":");

    /** Prepend all the variables in other to the variables in this.
     *
     *  This is done by calling prependUnique for each of the variables individually. */
    void prependUnique(const Environment &other);

    /** Export environment variables to the environment.
     *
     *  This completely rewrites the process environment. Any variables that were there before will be erased and replaced with
     *  variables only from this object. */
    void exportVars() const;
};

} // namespace

#endif
