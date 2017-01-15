#ifndef Spock_PackagePattern_H
#define Spock_PackagePattern_H

#include <Spock/VersionNumber.h>

namespace Spock {

/** A pattern that matches against packages.
 *
 *  Matching is against names, version numbers, and/or hashes. */
class PackagePattern {
public:
    enum VersOp { VERS_EQ, VERS_NE, VERS_LT, VERS_LE, VERS_GT, VERS_GE, VERS_HY };

private:
    std::string pkgName_;
    VersOp versOp_;
    VersionNumber version_;
    std::string hash_;

public:
    /** Default constructed pattern matches all packages. */
    PackagePattern()
        : versOp_(VERS_EQ) {}

    /** Create a pattern from a string.
     *
     *  The string has the following form: {NAME}{VERSION}{HASH}.  Each of the parts are optional.
     *
     *  The {NAME} can include '*' which matches zero or more characters. Otherwise, names consist of alphanumeric characters,
     *  and letters from the set "_", "-", "+".
     *
     *  The {VERSION} begins with a relational operator: "=", "<", "<=", ">", ">=", "!=", or "-".  Except for "-",
     *  they have the usual meanings as defined by the @ref VersionNumber class. The "-" operator is similar to "=" except it
     *  will also match version numbers that have additional parts. For example, "-2.5" will not only match "2.5" but also
     *  "2.5.0", "2.5.1", etc.
     *
     *  The {HASH} is an eight-character hexadecimal number introduced with an "@9cb9b45d". */
    PackagePattern(const std::string&);

    /** Parse a string to initialize this pattern. */
    void parse(const std::string&);

    /** The package name part of a pattern. */
    const std::string& name() const { return pkgName_; }

    /** Version comparison operator. */
    const VersOp versionComparison() const { return versOp_; }

    /** The version number part of a pattern. */
    const VersionNumber& version() const { return version_; }

    /** The hash part of a pattern. */
    const std::string& hash() const { return hash_; }

    /** Convert pattern to string. */
    std::string toString() const;

    /** Does the pattern match a package? */
    bool matches(const PackagePtr&) const;

    /** Does the pattern match a version number? */
    bool matches(const VersionNumber&) const;
};

} // namespace

#endif
