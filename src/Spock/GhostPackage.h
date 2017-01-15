#ifndef Spock_GhostPackage_H
#define Spock_GhostPackage_H

#include <Spock/Package.h>

namespace Spock {

/** Represents a package which isn't installed.
 *
 *  A ghost package behaves like an installed package (InstalledPackage) except there's no actual package installed. Instead,
 *  ghost packages are generated as placeholders from package definitions (DefinedPackage) to represent something that
 *  <em>could</em> be installed. Since ghost packages aren't actually installed, they have no hash; but they do always have a
 *  name and one or more exact version numbers. */
class GhostPackage: public Package {
    DefinedPackagePtr defn_;
    VersionNumbers versions_;

public:
    typedef Sawyer::SharedPointer<GhostPackage> Ptr;

protected:
    GhostPackage();
    GhostPackage(const DefinedPackagePtr &definition, const VersionNumbers &versions);

public:
    ~GhostPackage();

    virtual bool isInstalled() const { return false; }
    virtual std::vector<PackagePattern> dependencyPatterns() const;
    virtual VersionNumber version() const;
    virtual void version(const VersionNumber&);
    virtual VersionNumbers versions() const;
    virtual std::string toString() const;

    /** Create a new instance.
     *
     *  This new instance represents a single package capable of installing any of the specified versions. Each version has
     *  identical dependencies, etc. The version number set cannot be empty. */
    static Ptr instance(const DefinedPackagePtr&, const VersionNumbers&);

    /** Create another instance but different versions.
     *
     *  This is like a copy constructor, but it changes the versions at the same time.  The new versions must be a non-empty
     *  subset of the original versions. */
    static Ptr instance(const Ptr&, const VersionNumbers&);

    /** True if version is present in package. */
    bool isValidVersion(const VersionNumber&) const;

    /** Return a version prefix.
     *
     *  The version prefix consists of those version parts that are the same across all versions of this package. */
    VersionNumber versionPrefix() const;

    /** Package definition. */
    const DefinedPackagePtr& definition() const { return defn_; }

    /** Create parasite instances. */
    Packages parasites() const;

    /** True if this instance is a parasite. */
    bool isParasite() const;

    /** Install the package and its parasites.
     *
     *  Since dependencies have to be installed first, and since a parasite depends on its host, we can be sure that the host
     *  is already installed. When installing a package with parasites, the parasites are returned by the @p parasites
     *  argument. */
    PackagePtr install(Context &ctx, const VersionNumber&, Packages &parasites /*out*/);
};

} // namespace

#endif
