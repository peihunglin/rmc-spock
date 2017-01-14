#ifndef Spock_Context_H
#define Spock_Context_H

#include <Spock/Spock.h>

#include <boost/filesystem.hpp>

namespace Spock {

/** Runtime context. */
class Context {
    boost::filesystem::path rootdir_;                   // root of spock installation to provide defaults for members
    boost::filesystem::path bindir_;                    // directory holding spock's executables
    boost::filesystem::path vardir_;                    // directory holding spock's database
    std::vector<InstalledPackagePtr> employed_;         // installed packages that are being used

    SearchPaths searchPaths_;                           // things like PATH, LD_LIBRARY_PATH, etc.

public:
    /** Obtain context from environment. */
    Context();
    ~Context();

    /** Get the name of an installed config file.
     *
     *  The file need not exist yet. */
    boost::filesystem::path installedConfig(const std::string &hash) const;

    /** Insert into environment variables.
     *
     *  For each of the environment variables specified, append their values to the ends of environment variables in this
     *  context. For instance, if the insertion is for the variable PATH with the values "dirX" and "dirY" and this context
     *  already stores the value "dirA", then upon return the environment's PATH will have "dirA", "dirX", and "dirY" in that
     *  order.  Duplicate values are not removed. Variables that didn't previously exist are created. When running a subshell,
     *  these values are colon-separated, as in "dirA:dirX:dirY". */
    void insertPaths(const SearchPaths&);

    /** List of employed packages. */
    const std::vector<InstalledPackagePtr>& employed() const { return employed_; }

    /** Cause a package to be employed.
     *
     *  The installed package (specified by hash or pointer) is added to the list of employed packages. This is a no-op if the
     *  exact package hash is already employed. If an error occurs, such as a conflict between the requested package or its
     *  dependencies and some package that is already employed, then an exception is thrown and the context is not modified.
     *
     *  Returns true if the context changed, false otherwise.
     *
     * @{ */
    bool employ(const std::string &hash);
    bool employ(const InstalledPackagePtr&);
    /** @} */

    /** Set environment variables based on this context. */
    void setEnvironment() const;

    /** Run a command in a subshell.
     *
     *  A subshell is created based on this context, and the command is run in that subshell.  If no command is specified then
     *  an interactive subshell is run.  The return value is the exit status of the command. */
    int subshell(const std::vector<std::string> &command) const;

private:
    // Version of employ which is not exception safe
    bool employNS(const InstalledPackagePtr&);
};

} // namespace

#endif

