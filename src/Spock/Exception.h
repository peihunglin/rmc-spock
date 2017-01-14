#ifndef Spock_Exception_H
#define Spock_Exception_H

#include <Spock/Spock.h>

namespace Spock {

/** Exceptions thrown by this library. */
namespace Exception {

class SpockError: public std::runtime_error {
public:
    explicit SpockError(const std::string &what)
        : std::runtime_error(what) {}
};

/** A problem with the environment. */
class EnvironmentError: public SpockError {
public:
    explicit EnvironmentError(const std::string &what)
        : SpockError(what) {}
};

class Conflict: public SpockError {
public:
    explicit Conflict(const std::string &what)
        : SpockError(what) {}
};

class ResourceError: public SpockError {
public:
    explicit ResourceError(const std::string &what)
        : SpockError(what) {}
};

class NotFound: public SpockError {
public:
    explicit NotFound(const std::string &what)
        : SpockError(what) {}
};

class SyntaxError: public SpockError {
public:
    explicit SyntaxError(const std::string &what)
        : SpockError(what) {}
};

} // namespace
} // namespace

#endif
