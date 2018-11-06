#ifndef Spock_Package_H
#define Spock_Package_H

#include <Spock/Spock.h>
#include <Spock/VersionNumber.h>

namespace Spock {

/** Represents an installed package or a package that could be installed. */
class Package: public Sawyer::SharedObject {
public:
    /** Reference-counting pointer. */
    typedef Sawyer::SharedPointer<Package> Ptr;

protected:
    std::string hash_;                                  // unique identification for this installation of this package
    std::string name_;                                  // name of package, such as "boost"
    Aliases aliases_;                                   // secondary names

    Package();                                          // to be usable in std::vector

public:
    virtual ~Package();

    /** Predicate: True if package is installed. */
    virtual bool isInstalled() const = 0;

    /** Unique identification for this package.
     *
     *  @{ */
    std::string hash() const /*final*/ { return hash_; }
    void hash(const std::string&) /*final*/;
    /** @} */

    /** Primary name of the package without version.
     *
     *  Name of the installed package without a version number, such as "boost" or "yaml-cpp".
     *
     * @{ */
    const std::string& name() const /*final*/{ return name_; }
    void name(const std::string&) /*final*/;
    /** @} */

    /** Names in common to both packages.
     *
     *  Returns the set of names (primary names and aliases) that are common to both packages. */
    Aliases namesInCommon(const PackagePtr&) const;

    /** Primary Version number.
     *
     *  Installed packages have only one version number and that's what's returned.  Ghost packages are placeholders for one or
     *  more versions, and querying the version returns the best (highest) version. Setting the version of a ghost package adds
     *  the specified version to the set of versions.
     *
     * @{ */
    virtual VersionNumber version() const = 0;
    virtual void version(const VersionNumber&) = 0;
    /** @} */

    /** All versions.
     *
     *  An installed package has only one version, but a ghost package may have more than one. */
    virtual VersionNumbers versions() const;

    /** Name, version, and hash. */
    virtual std::string toString() const;

    /** Name, version, and hash, with ANSI color escapes. */
    virtual std::string toStringColored() const;

    /** Secondary names.
     *
     *  Every package has a primary name and zero or more secondary names (aliases).  When searching for packages, aliases work
     *  the same as primary names. If two packages with different primary names have an alias in comment then it is not
     *  possible to use both packages at once.  For instance, gnu-c++11 and gnu-c++03 packages both have aliases c++-compiler
     *  which prevents them from both being used at the same time.
     *
     * @{ */
    const Aliases& aliases() const { return aliases_; }
    void aliases(const Aliases &set) { aliases_ = set; }
    /** @} */

    /** Names of dependencies.
     *
     *  Although these are returned as patterns, they each include a hash which makes them match only one installed package.
     *  These are the dependencies read from the YAML file. */
    virtual std::vector<PackagePattern> dependencyPatterns() const = 0;

    /** True if two packages are identical.
     *
     *  Packages are identical if they're the same object or they have the same non-empty hash, or they have the same name and
     *  version(s). */
    bool identical(const PackagePtr &other) const;

    /** True if two packages are mutually exclusive.
     *
     *  Two packages are mutually exclusive if they cannot be used at the same time. This determination is made by looking only
     *  at the package itself and not at any dependencies or constraints. */
    bool excludes(const PackagePtr &other) const;
};

} // namespace

#endif

