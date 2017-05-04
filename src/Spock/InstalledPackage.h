#ifndef Spock_InstalledPackage_H
#define Spock_InstalledPackage_H

#include <Spock/Context.h>
#include <Spock/Environment.h>
#include <Spock/Package.h>
#include <Spock/PackagePattern.h>
#include <Spock/VersionNumber.h>

#include <boost/date_time/posix_time/posix_time_types.hpp>

namespace Spock {

/** Represents a package that has been installed. */
class InstalledPackage: public Package {
    VersionNumber version_;
    std::vector<PackagePattern> dependencyPatterns_;
    std::vector<GlobalFlagPtr> flags_;
    boost::posix_time::ptime timestamp_;
    Environment environmentSearchPaths_;

protected:
    InstalledPackage();
    
public:
    ~InstalledPackage();
    
    /** Reference-counting pointer. */
    typedef Sawyer::SharedPointer<InstalledPackage> Ptr;

    /** Create object from a hash or package specification that includes a hash. */
    static Ptr instance(const Context&, const std::string &spec);

    /** Create object from a hash but override the configuration file. */
    static Ptr instance(const Context&, const std::string &hash, const boost::filesystem::path&);

    /** Create default-constructed object.
     *
     *  This constructor is used when installing a package: a default-constructed object is created, filled in with properties
     *  about the installation, and then written to a YAML configuration file. */
    static Ptr instance();

    virtual bool isInstalled() const { return true; }
    virtual VersionNumber version() const;
    virtual void version(const VersionNumber&);

    virtual std::vector<PackagePattern> dependencyPatterns() const { return dependencyPatterns_; }

    /** Global flags.
     *
     *  The global flags affect things like compilation mode.
     *
     * @{ */
    const std::vector<GlobalFlagPtr>& flags() const { return flags_; }
    void flags(const std::vector<GlobalFlagPtr>&);
    /** @} */

    /** Time stamp stored in config file.
     *
     *  The time stamp represents when the package was installed.
     *
     *  @{ */
    const boost::posix_time::ptime& timestamp() const { return timestamp_; }
    void timestamp(const boost::posix_time::ptime&);
    /** @} */

    /** Environment variable search paths.
     *
     *  These are the values for variables like $PATH and $LD_RUN_PATH.
     *
     * @{ */
    const Environment& environmentSearchPaths() const { return environmentSearchPaths_; }
    void environmentSearchPaths(const Environment&);
    /** @} */

    /** Remove the specified package from the operating system.
     *
     *  This function is low-level--it does not attempt to remove packages that may have depended on this package, nor does it
     *  update the context in order to de-register the removed package. It removes the YAML config file first, then the
     *  installed package, so that if the removal fails or is interrupted we're not left with a partially installed package.
     *
     *  The package object itself will still exist while there are references to it. */
    void remove(Context&);
};

} // namespace

#endif
