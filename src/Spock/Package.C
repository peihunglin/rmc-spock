#include <Spock/Package.h>

namespace Spock {

Package::Package() {}

Package::~Package() {}

void
Package::hash(const std::string &s) {
    ASSERT_require(isHash(s));
    hash_ = s;
}

void
Package::name(const std::string &s) {
    ASSERT_forbid(s.empty());
    name_ = s;
}

std::string
Package::toString() const {
    std::string s = name();
    if (!version().isEmpty())
        s += "=" + version().toString();
    if (!hash().empty())
        s += "@" + hash();
    if (s.empty())
        s = "empty";
    return s;
}

std::string
Package::toStringColored() const {
    bool useColor = isatty(1);
    std::string s = name();
    if (!version().isEmpty()) {
        if (useColor) {
            s += "\033[36m=" + version().toString() + "\033[0m";;
        } else {
            s += "=" + version().toString();
        }
    }
    if (!hash().empty()) {
        if (useColor) {
            s += "\033[33m@" + hash() + "\033[0m";
        } else {
            s += "@" + hash();
        }
    }
    if (s.empty())
        s = "empty";
    return s;
}

VersionNumbers
Package::versions() const {
    VersionNumbers retval;
    retval.insert(version());
    return retval;
}

bool
Package::identical(const Package::Ptr &other) const {
    if (getRawPointer(other) == this)
        return true;

    if (!hash().empty())
        return hash() == other->hash();

    if (name() != other->name())
        return false;

    return versions() == other->versions();
}

Sawyer::Container::Set<std::string>
Package::namesInCommon(const Package::Ptr &other) const {
    ASSERT_not_null(other);

    Aliases n1 = aliases();
    n1.insert(name());

    Aliases n2 = other->aliases();
    n2.insert(other->name());

    return n1 & n2;
}

bool
Package::excludes(const Package::Ptr &other) const {
    if (name() != other->name())
        return !namesInCommon(other).isEmpty();         // overlapping aliases
        
    if (!hash().empty() && !other->hash().empty())
        return hash() != other->hash();                 // unequal, non-empty hashes
    
    if (!version().isEmpty() && !other->version().isEmpty()) {
        VersionNumbers v1 = versions();
        VersionNumbers v2 = other->versions();
        return (v1 & v2).isEmpty();                     // versions specified but disjoint
    }

    return false;
}

} // namespace
