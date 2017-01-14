#ifndef Spock_InstalledPackage_H
#define Spock_InstalledPackage_H

#include <Spock/Context.h>
#include <Spock/VersionNumber.h>

#include <boost/date_time/posix_time/posix_time_types.hpp>

namespace Spock {

/** Represents a package that has been installed. */
class InstalledPackage: public Sawyer::SharedObject {
    std::string hash_;                                  // unique identification for this installation of this package
    std::string name_;                                  // name of package, such as "boost"
    VersionNumber version_;
    std::vector<InstalledPackagePtr> dependencies_;
    std::vector<GlobalFlagPtr> flags_;
    boost::posix_time::ptime timestamp_;
    SearchPaths environmentSearchPaths_;

protected:
    InstalledPackage();
    
public:
    ~InstalledPackage();
    
    /** Reference-counting pointer. */
    typedef Sawyer::SharedPointer<InstalledPackage> Ptr;

    /** Create by reading a configuration file. */
    static Ptr instance(const boost::filesystem::path&);

    /** Create object from a configuration file hash. */
    static Ptr instance(const Context &ctx, const std::string &hash);

    /** Create default-constructed object.
     *
     *  This constructor is used when installing a package: a default-constructed object is created, filled in with properties
     *  about the installation, and then written to a YAML configuration file. */
    static Ptr instance();

    /** Unique identification for this package.
     *
     *  @{ */
    std::string hash() const { return hash_; }
    void hash(const std::string&);
    /** @} */

    /** Name of the package without version.
     *
     *  Name of the installed package without a version number, such as "boost" or "yaml-cpp".
     *
     * @{ */
    const std::string& name() const { return name_; }
    void name(const std::string&);
    /** @} */

    /** Version number that is installed.
     *
     * @{ */
    const VersionNumber& version() const { return version_; }
    void version(const VersionNumber&);
    /** @} */

    /** Name, version, and hash. */
    std::string fullName() const;

    /** Direct runtime dependencies.
     *
     * @{ */
    const std::vector<InstalledPackage::Ptr>& dependencies() const { return dependencies_; }
    void dependencies(const std::vector<InstalledPackage::Ptr>&);
    /** @} */

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
     *  These are the values for variables like $PATH and $LD_LIBRARY_PATH.
     *
     * @{ */
    const SearchPaths& environmentSearchPaths() const { return environmentSearchPaths_; }
    void environmentSearchPaths(const SearchPaths&);
    /** @} */
};

} // namespace

#endif
