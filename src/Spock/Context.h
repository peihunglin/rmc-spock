#ifndef Spock_Context_H
#define Spock_Context_H

#include <Spock/Directory.h>
#include <Spock/Environment.h>

#include <boost/filesystem.hpp>
#include <Sawyer/Graph.h>

namespace Spock {

/** Runtime context. */
class Context {
public:
    typedef Sawyer::Container::Graph<std::string, Sawyer::Nothing, std::string> Lattice;

private:
    typedef Sawyer::Container::Map<std::string /*name*/, DefinedPackagePtr> DefinitionsByName;

    struct EnvStackItem {
        Environment variables;
        Packages packages;
    };

    boost::filesystem::path rootdir_;                   // root of spock installation to provide defaults for members
    boost::filesystem::path bindir_;                    // directory holding spock's executables
    boost::filesystem::path vardir_;                    // directory holding spock's runtime database
    boost::filesystem::path optdir_;                    // directory where spock installs packages
    boost::filesystem::path pkgdir_;                    // directory holding definitions of packages to be installed
    boost::filesystem::path scriptdir_;                 // file containing shell scripts
    boost::filesystem::path builddir_;                  // directory where building of packages takes place
    Directory allPackages_;                             // database of all known packages, installed or not
    PackagePtr spockItself_;                            // pseudo-package for spock itself
    DefinitionsByName definitionsByName_;               // all known package definitions indexed by their name
    std::vector<EnvStackItem> envStack_;                // stack of environments

public:
    static Sawyer::Message::Facility mlog;

    enum CommandStatus { COMMAND_SUCCESS=0, COMMAND_FAILED=1, COMMAND_NOT_RUN=2 };

    // Remembers the size of the context environment stack and restores it upon destruction.
    class SavedStack {
        Context &ctx_;
        size_t stackSize_;
    public:
        explicit SavedStack(Context &ctx);
        ~SavedStack();
    };
    
    /** Obtain context from environment. */
    Context();
    ~Context();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Directories and file names
public:
    /** Name of the root directory for other spock directories. */
    boost::filesystem::path rootDirectory() const;

    /** Name of directory containing spock binary executables. */
    boost::filesystem::path binDirectory() const;

    /** Name of top-level directory containing runtime information. */
    boost::filesystem::path varDirectory() const;

    /** Name of directory where packages are installed. */
    boost::filesystem::path optDirectory() const;

    /** Name of directory containing descriptions of packages that could be installed. */
    boost::filesystem::path packageDirectory() const;

    /** Name of file containing shell scripts. */
    boost::filesystem::path scriptDirectory() const;

    /** Directory that caches downloaded files. */
    boost::filesystem::path downloadDirectory() const;

    /** Directory where building of packages occurs. */
    boost::filesystem::path buildDirectory() const;

    /** Get the name of an installed config file.
     *
     *  The file need not exist yet. */
    boost::filesystem::path installedConfig(const std::string &hash) const;

    /** Push a new environment onto the stack by duplicating the top item. */
    void pushEnvironment();

    /** Pop the top environment from the stack. */
    void popEnvironment();

    /** Number of environments on the stack. */
    size_t environmentStackSize() const { return envStack_.size(); }

    /** Insert packages into the top-of-stack environment.
     *
     *  It is not possible to remove packages from employment. In order to acheive that effect, use employment stacks.  The
     *  specified packages must all be installed packages.
     *
     *  Inserting new packages also adjusts environment variables. The SPOCK_EMPLOYED variable is extended with the hashes of
     *  the new packages, and the packages' environment definitions might make other changes as well.
     *
     *  Packages that are already employed have no further effect.
     *
     * @{ */
    void insertEmployed(const Packages&);
    void insertEmployed(const PackagePtr&);
    /** @} */

    /** Packages employed in the top-of-stack environment. */
    const Packages& employed() const;

    /** True if package is currently employed. */
    bool isEmployed(const PackagePtr&) const;

    /** Assign a value to an environment variable.
     *
     *  Assigns the specified value to the environment variable at the top of the environment stack. */
    void setEnvVar(const std::string &variable, const std::string &value);

    /** Set environment variables based on this context. */
    void setEnvironment() const;

    struct SubshellSettings {
        std::string progressName;                       // optional: what to show for the progress bar
        boost::filesystem::path output;                 // optional: where to save output

        SubshellSettings() {}
        SubshellSettings(const std::string &name): progressName(name) {}
    };

    /** Run a command in a subshell.
     *
     *  A subshell is created based on this context, and the command is run in that subshell.  If no command is specified then
     *  an interactive subshell is run. */
    CommandStatus subshell(const std::vector<std::string> &command, const SubshellSettings &settings = SubshellSettings()) const;
    CommandStatus subshell(const boost::filesystem::path &exe, const SubshellSettings &settings = SubshellSettings()) const;

    /** Find the installed pseudo package representing Spock itself. */
    PackagePtr spockItself() const;

    /** Scans the installed packages directory and loads all of them into memory. */
    void scanInstalledPackages();

    /** Deregister an installed package. This makes it so the package will no longer be found by findInstalled. */
    void deregister(const PackagePtr&);

    /** Scans a newly installed package. */
    PackagePtr scanInstalledPackage(const PackagePattern&);
    
    /** Find packages matching pattern, installed or not. */
    Packages findPackages(const PackagePattern&) const;

    /** Find an installed package according to a pattern. */
    Packages findInstalled(const PackagePattern&) const;

    /** Find ghost (uninstalled) packages according to a pattern. */
    Packages findGhosts(const PackagePattern&) const;

    /** Find information about installing some package.
     *
     *  Returns the package installation definition if found, or else null. */
    DefinedPackagePtr findDefined(const PackagePattern&);

    /** Dependencies of a package.
     *
     *  Installed packages have only installed dependencies. Ghost package dependencies can be installed or not. */
    Packages packageDependencies(const PackagePtr&) const;

    /** Create a graph showing the dependency relationships of packages */
    static Lattice dependencyLattice(const Packages&);

    /** Create a GraphViz representation to the specified file. */
    static std::string toGraphViz(const Lattice&);

    /** Sort packages so dependencies come before things that depend on them. */
    static void sortByDependencyLattice(Packages&);

private:
    // Add ghost packages to the mix
    void scanGhostPackages();

    std::string osCharacteristics();
    PackagePtr findOrCreateSelf();
    
};

} // namespace

#endif

