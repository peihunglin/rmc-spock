#include <Spock/VersionNumber.h>

#include <algorithm>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>

namespace Spock {

VersionNumber::VersionNumber(std::string s) {
    boost::trim(s);
    if (!s.empty())
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
VersionNumber::operator-(const VersionNumber &other) const {
    if (size() < other.size())
        return false;
    ASSERT_require(parts_.size() >= other.parts_.size());
    return std::equal(other.parts_.begin(), other.parts_.end(), parts_.begin());
}

// Whole number version part? Returns a whole number or -1
int wholeNumber(const std::string &str) {
    if (str.empty() || str.size() > 6)
        return -1;
    BOOST_FOREACH (char ch, str) {
        if (!isdigit(ch))
            return -1;
    }
    return strtoul(str.c_str(), NULL, 10);
}

bool
VersionNumber::operator<(const VersionNumber &other) const {
    for (size_t i=0; i<parts_.size() && i < other.parts_.size(); ++i) {
        int w0 = wholeNumber(parts_[i]);
        int w1 = wholeNumber(other.parts_[i]);
        if (w0 >= 0 && w1 >= 0) {                       // compare as numbers
            if (w0 != w1)
                return w0 < w1;
        } else {                                        // compare as strings
            if (parts_[i] != other.parts_[i])
                return parts_[i] < other.parts_[i];
        }
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
