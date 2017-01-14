#include <Spock/Context.h>

#include <Spock/Exception.h>
#include <Spock/InstalledPackage.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>
#include <sys/wait.h>

namespace Spock {

// For exception safety without interfering with debugging
class SaveRestore {
    Context &source_;
    Context saved_;
    bool discard_;
public:
    explicit SaveRestore(Context &c)
        : source_(c), saved_(c), discard_(false) {}
    ~SaveRestore() {
        if (!discard_)
            source_ = saved_;
    }
    void discard() {
        discard_ = true;
    }
};

// Save and restore a Context object for exception safety
Context::Context() {
    if (const char *s = getenv("SPOCK_ROOT"))
        rootdir_ = s;

    if (const char *s = getenv("SPOCK_BIN")) {
        bindir_ = s;
    } else if (!rootdir_.empty()) {
        bindir_ = rootdir_ / "bin";
    } else {
        // leave bindir_ empty and let exec look it up in $PATH
    }
    
    if (const char *s = getenv("SPOCK_VAR")) {
        vardir_ = s;
    } else if (!rootdir_.empty()) {
        vardir_ = rootdir_ / "var";
    } else {
        vardir_ = "/var/spock";
    }

    if (const char *s = getenv("SPOCK_EMPLOYED")) {
        std::string ss = s;
        std::vector<std::string> hashes;
        boost::split(hashes, ss, boost::is_any_of(":-, \t"));
        BOOST_FOREACH (std::string &hash, hashes)
            employed_.push_back(InstalledPackage::instance(*this, hash));
    }
}

Context::~Context() {}

boost::filesystem::path
Context::installedConfig(const std::string &hash) const {
    return vardir_ / "installed" / (hash + ".yaml");
}

void
Context::insertPaths(const SearchPaths &newPaths) {
    BOOST_FOREACH (const SearchPaths::Node &node, newPaths.nodes()) {
        if (!searchPaths_.exists(node.key())) {
            if (const char *s = getenv(node.key().c_str())) {
                std::vector<std::string> parts;
                boost::split(parts, s, boost::is_any_of(":"));
                searchPaths_.insert(node.key(), parts);
            }
        }
        std::vector<std::string> &paths = searchPaths_.insertMaybeDefault(node.key());
        paths.insert(paths.end(), node.value().begin(), node.value().end());
    }
}

// Exception safe
bool
Context::employ(const std::string &hash) {
    return employ(InstalledPackage::instance(*this, hash));
}

// Exception safe
bool
Context::employ(const InstalledPackage::Ptr &pkg) {
    ASSERT_not_null(pkg);
    SaveRestore sr(*this);
    bool retval = employNS(pkg);
    sr.discard();
    return retval;
}

// Not exception safe
bool
Context::employNS(const InstalledPackage::Ptr &pkg) {
    // We can't already be employing the same package with a different version or configuration.  But if we're
    // trying to add something that's already employed then we don't have to do anything.
    BOOST_FOREACH (const InstalledPackage::Ptr &inUse, employed()) {
        if (pkg->hash() == inUse->hash())
            return false;
        if (pkg->name() == inUse->name())
            throw Exception::Conflict("cannot use " + pkg->fullName() + " since " + inUse->fullName() + " is already employed");
    }

    // Process this package's environment settings
    insertPaths(pkg->environmentSearchPaths());
    employed_.push_back(pkg);

    // Recursively employ the dependencies, if any
    BOOST_FOREACH (InstalledPackage::Ptr dep, pkg->dependencies())
        employNS(dep);
    return true;
}

void
Context::setEnvironment() const {
    // Miscellaneous environment variables like PATH, LD_LIBRARY_PATH, etc.
    BOOST_FOREACH (const SearchPaths::Node &node, searchPaths_.nodes()) {
        if (node.value().empty()) {
            unsetenv(node.key().c_str());
        } else {
            std::string value = boost::join(node.value(), ":");
            setenv(node.key().c_str(), value.c_str(), true /*overwrite*/);
        }
    }

    // Predefined SPOCK_* environment variables
    std::vector<std::string> employedVersions;
    BOOST_FOREACH (const InstalledPackage::Ptr &pkg, employed_)
        employedVersions.push_back(pkg->hash());
    if (employedVersions.empty()) {
        unsetenv("SPOCK_EMPLOYED");
    } else {
        setenv("SPOCK_EMPLOYED", boost::join(employedVersions, ":").c_str(), true /*overwrite*/);
    }

    if (rootdir_.empty()) {
        unsetenv("SPOCK_ROOT");
    } else {
        setenv("SPOCK_ROOT", rootdir_.string().c_str(), true /*overwrite*/);
    }

    if (bindir_.empty()) {
        unsetenv("SPOCK_BIN");
    } else {
        setenv("SPOCK_BIN", bindir_.string().c_str(), true /*overwrite*/);
    }

    if (vardir_.empty()) {
        unsetenv("SPOCK_VAR");
    } else {
        setenv("SPOCK_VAR", vardir_.string().c_str(), true /*overwrite*/);
    }
}

int
Context::subshell(const std::vector<std::string> &command) const {
    setEnvironment();

    char **argv = new char*[4 + command.size()];
    int argc = 0;
    argv[argc++] = strdup("bash");
    if (!command.empty()) {
        argv[argc++] = strdup("-c");
        BOOST_FOREACH (const std::string &s, command)
            argv[argc++] = strdup(s.c_str());
    }
    argv[argc] = NULL;

    int status = 0;
    pid_t child = fork();
    if (-1 == child) {
        throw Exception::ResourceError("fork failed: " + std::string(strerror(errno)));
    } else if (child) {
        // This is the parent process
        if (-1 == TEMP_FAILURE_RETRY(waitpid(child, &status, 0)))
            throw Exception::ResourceError("wait process " + boost::lexical_cast<std::string>(child) + ": " + strerror(errno));
    } else {
        // This is the child process
        execv("/bin/bash", argv);
        throw Exception::ResourceError("exec failed for /bin/bash: " + std::string(strerror(errno)));
    }
    
    return status;
}

} // namespace
