#ifndef Spock_VersionNumber_H
#define Spock_VersionNumber_H

#include <Spock/Spock.h>


namespace Spock {

/** Dotted version number.
 *
 *  A version number is a juxtaposition of parts separated from one another by dots, as in "1.56.4-alpha" where the parts are
 *  "1", "56", and "4-alpha". */
class VersionNumber {
    std::vector<std::string> parts_;

public:
    /** Constructs an empty version number. */
    VersionNumber() {}

    /** Construct from string. */
    /*implicit*/ VersionNumber(const std::string &s);

    /** Predicate for emptiness.
     *
     *  A version number that has not parts, such as a default constructed version number, is empty. */
    bool isEmpty() const {
        return parts_.empty();
    }

    /** Number of parts in a version number. */
    size_t size() const {
        return parts_.size();
    }

    /** Juxtapose two version numbers.
     *
     *  The return value is a version number whose initial parts are from @p this version and whose subsequent parts are from
     *  the @p other version. */
    VersionNumber operator+(const VersionNumber &other) const;

    /** In-place juxtaposition.
     *
     *  Appends the parts of the @p other version to the end of @p this version. */
    VersionNumber& operator+=(const VersionNumber &other);

    /** Version equality.
     *
     *  Two versions are equal if they have the same number of parts and the corresponding parts are equal.
     *
     * @{ */
    bool operator==(const VersionNumber &other) const;
    bool operator!=(const VersionNumber &other) const;
    /** @} */

    /** Version comparison.
     *
     *  Returns true if @p this version is less than the @p other version. The comparison is made by comparing each
     *  corresponding pair of parts and returnnig true or false for the first unequal pair. If all corresponding pairs are
     *  equal but one version has fewer parts than the other, then the version with fewer parts is less than the version with
     *  more parts. */
    bool operator<(const VersionNumber &other) const;

    /** Does a version satisfy some requirement.
     *
     *  Returns true if @p this version requirement is satisfied by @p other version.  For instance, a requirement of 1.2 is
     *  satisfied by 1.2, 1.2.0, 1.3, etc. but not by 1.1 or 1 or an empty version. In essence, this method is just another
     *  name for the less-than operator. */
    bool isSatisfiedBy(const VersionNumber &other) const;

    /** Convert a version number to a string. */
    std::string toString() const;
};

} // namespace

#endif

