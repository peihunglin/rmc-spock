#include <Spock/VersionNumber.h>

#include <algorithm>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>

namespace Spock {

VersionNumber::VersionNumber(const std::string &s) {
    boost::split(parts_, s, boost::is_any_of("."));
}

VersionNumber
VersionNumber::operator+(const VersionNumber &other) const {
    VersionNumber retval = *this;
    return retval += other;
}

VersionNumber&
VersionNumber::operator+=(const VersionNumber &other) {
    parts_.insert(parts_.end(), other.parts_.begin(), other.parts_.end());
    return *this;
}

bool
VersionNumber::operator==(const VersionNumber &other) const {
    if (parts_.size() != other.parts_.size())
        return false;
    return std::equal(parts_.begin(), parts_.end(), other.parts_.begin());
}

bool
VersionNumber::operator!=(const VersionNumber &other) const {
    return !(*this == other);
}

bool
VersionNumber::operator<(const VersionNumber &other) const {
    for (size_t i=0; i<parts_.size() && i < other.parts_.size(); ++i) {
        if (parts_[i] != other.parts_[i])
            return parts_[i] < other.parts_[i];
    }
    if (parts_.size() != other.parts_.size())
        return parts_.size() < other.parts_.size();
    return false;
}

bool
VersionNumber::isSatisfiedBy(const VersionNumber &other) const {
    return *this < other || *this == other;
}

std::string
VersionNumber::toString() const {
    if (isEmpty())
        return "none";
    return boost::join(parts_, ".");
}


} // namespace
