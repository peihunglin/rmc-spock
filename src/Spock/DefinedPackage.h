#ifndef Spock_DefinedPackage_H
#define Spock_DefinedPackage_H

#include <Spock/Context.h>
#include <Spock/TemporaryDirectory.h>
#include <Spock/VersionNumber.h>

#include <yaml-cpp/yaml.h>

namespace Spock {

/** An installable package. */
class DefinedPackage: public Sawyer::SharedObject {
public:
    typedef Sawyer::SharedPointer<DefinedPackage> Ptr;
    static Sawyer::Message::Facility mlog;

    struct Settings {
        VersionNumber version;                          // version number being installed
        std::string hash;                               // hash assigned for this installation
        bool quiet;                                     // if set, then shell scripts produce no output to the tty
        bool keepTempFiles;                             // do not delete temporary files and directories?
        boost::filesystem::path installDirOverride;     // to override the usual $BOOST_ROOT/var/installed
        Packages parasites;                             // parasites also installed when the host was installed

        Settings(): quiet(true), keepTempFiles(false) {}
    };

private:
    std::string name_;
    boost::filesystem::path configFile_;
    YAML::Node config_;
    VersionNumbers versions_;

protected:
    // Opens the specified YAML config file for reading and verifies that some things are correct.
    DefinedPackage(const std::string &pkgName, const boost::filesystem::path &configFile);

public:
    ~DefinedPackage();

    /** Obtains a package definition by reading a configuration file.
     *
     *  This is a low-level function. It is better to obtain package definitions by the Context object, which also caches
     *  them. */
    static Ptr instance(const std::string &pkgName, const boost::filesystem::path &configFile);

    /** Name of package.
     *
     *  Example, "boost".
     *
     * @{ */
    const std::string& name() const { return name_; }
    /** @} */

    /** Versions supported by this package. */
    const VersionNumbers& versions() const { return versions_; }

    /** Supported versions organized by dependencies.
     *
     *  This returns the same version numbers as @ref versions, but groups them by dependencies. Versions that have
     *  the same dependencies are in the same group. */
    std::vector<VersionNumbers> versionsByDependency() const;

    /** Test whether a version is supported.
     *
     *  The specified version must be exact, i.e., "=" comparison. */
    bool isSupportedVersion(const VersionNumber&) const;

    /** Installation dependencies.
     *
     *  Since this is only a package definition and not an actual installed package, the dependencies are patterns rather than
     *  specific installations of packages. */
    std::vector<PackagePattern> dependencyPatterns(const VersionNumber &vers);
    
    /** Download the package from its upstream location.
     *
     *  Downloaded files are cached in $SPOCK_VAR/downloads, and the return value is the name of this file. The filenames in
     *  this directory follow the pattern PACKAGE-VERSION.tar.gz and usually untar into a "download" directory. */
    boost::filesystem::path download(Context&, const Settings&);

    /** Install the package.
     *
     *  Do all steps necessary to install a packge. The version number must be specified the @p settings, but the hash will be
     *  filled in by this function. */
    PackagePtr install(Context&, Settings &settings /*in,out*/);

    /** Patterns for describing parasites.
     *
     *  A "parasite" is another package definition whose targets are installed by this host definition. For instance, a GNU
     *  Compiler Collection package definition has a post-install section that installs various compilers, like gnu-c++03,
     *  gnu-gnu++03, gnu-c++11, gnu-gnu++11, gnu-c89, gnu-gnu89, etc.  The @p aliases parallels the return value and lists
     *  all aliases for each parasite. */
    std::vector<PackagePattern> parasitePatterns(const VersionNumber&, std::vector<Aliases> &aliases /*out*/) const;

private:
    // Full name of package as it would be installed
    std::string mySpec(const Settings&) const;

    // Name of cached download file (might not exist). Usually like "$SPOCK_VAR/downloads/PACKAGE-VERSION.tar.gz".
    boost::filesystem::path cachedDownloadFile(Context &ctx, const Settings&) const;

    // Find shell commands in the config file. Looks for config_[SECTIONNAME][VERSION].
    std::string findCommands(const std::string &sectionName, const VersionNumber&);


    // Return shell variables defined in the configuration for the selected version. Each member of the vector has the form
    // "NAME=VALUE".
    std::vector<std::string> shellVariables(const Settings&) const;

    // Create executable script to run commmands.  The script is created in the specified directory and has a standard prologue
    // to change to that directory and initialize certain variables.  Returns the name of the new script file.
    boost::filesystem::path createShellScript(const Settings&, const boost::filesystem::path &cwd, const std::string &commands,
                                              const std::vector<std::string> &extraVars = std::vector<std::string>()) const; 

    // Solve dependencies. The types are either "build" or "install" and matches the corresponding field in the YAML file.
    Packages solveDependencies(Context &ctx, const Settings&, const std::string &type1, const std::string &type2 = "");
    
    // Finish creating the user's YAML instlalation file and move it to its final destination.  The installDeps are those which
    // the solver chose and might include packages unrelated to the one being install (e.g., packages chosen by the user at a
    // higher level shell). Only a subset of the installDeps will make it into the YAML file
    void installConfigFile(Context&, const Settings&, const Packages &installDeps,
                           const boost::filesystem::path &yamlFile, const boost::filesystem::path &installDir);

    // Runs post-install commands, such as installing parasites.
    void postInstall(Context&, Settings&, const TemporaryDirectory &workingDir, const boost::filesystem::path &pkgRoot);
};

} // namespace

#endif
