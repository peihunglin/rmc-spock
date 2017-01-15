#include <Spock/Context.h>

#include <Spock/Exception.h>
#include <Spock/DefinedPackage.h>
#include <Spock/GhostPackage.h>
#include <Spock/InstalledPackage.h>
#include <Spock/PackagePattern.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread/thread.hpp>
#include <Sawyer/GraphAlgorithm.h>
#include <Sawyer/ProgressBar.h>
#include <fcntl.h>
#include <sys/wait.h>

using namespace Sawyer::Message::Common;

namespace Spock {

Sawyer::Message::Facility Context::mlog;

Context::SavedStack::SavedStack(Context &ctx)
    : ctx_(ctx), stackSize_(ctx.environmentStackSize()) {}

Context::SavedStack::~SavedStack() {
    while (ctx_.environmentStackSize() > stackSize_)
        ctx_.popEnvironment();
}

// Save and restore a Context object for exception safety
Context::Context() {
    envStack_.push_back(EnvStackItem());
    envStack_.back().variables.reload();

    // If SPOCK_VERSION is set then make sure it matches our own version.
    if (const char *s = getenv("SPOCK_VERSION")) {
        if (strcmp(s, VERSION))
            throw Exception::Conflict("this is spock " + std::string(VERSION) + " but $SPOCK_VERSION is set to " + s);
    } else {
        setEnvVar("SPOCK_VERSION", VERSION);
    }
    
    // The ROOT directory is the default prefix for most other directories
    if (const char *s = getenv("SPOCK_ROOT")) {
        rootdir_ = s;
    } else if (char *s = getenv("HOME")) {
        rootdir_ = boost::filesystem::path(s) / ".spock";
        setEnvVar("SPOCK_ROOT", rootdir_.string());
    } else {
        rootdir_ = "/opt/spock";
        setEnvVar("SPOCK_ROOT", rootdir_.string());
    }
    SAWYER_MESG(mlog[DEBUG]) <<"root directory (SPOCK_ROOT): " <<rootdir_ <<"\n";

    // The BIN directory holds Spock's compiled programs
    if (const char *s = getenv("SPOCK_BINDIR")) {
        bindir_ = s;
    } else {
        bindir_ = rootdir_ / "bin";
        setEnvVar("SPOCK_BINDIR", bindir_.string());
    }
    SAWYER_MESG(mlog[DEBUG]) <<"binary directory (SPOCK_BINDIR): " <<bindir_ <<"\n";

    // The SCRIPTS directory holds Spock's shell scripts
    if (const char *s = getenv("SPOCK_SCRIPTS")) {
        scriptdir_ = s;
    } else {
        scriptdir_ = rootdir_ / "scripts";
        setEnvVar("SPOCK_SCRIPTS", scriptdir_.string());
    }
    SAWYER_MESG(mlog[DEBUG]) <<"scripts directory (SPOCK_SCRIPTS): " <<scriptdir_ <<"\n";

    // The PKG directory is where descriptions of availalbe (not installed) packages are kept
    if (const char *s = getenv("SPOCK_PKGDIR")) {
        pkgdir_ = s;
    } else {
        pkgdir_ = rootdir_ / "lib" / "packages";
        setEnvVar("SPOCK_PKGDIR", pkgdir_.string());
    }
    SAWYER_MESG(mlog[DEBUG]) <<"package directory (SPOCK_PKGDIR): " <<pkgdir_ <<"\n";
        
    // The VAR directory holds run-time information that was not installed along with Spock
    if (const char *s = getenv("SPOCK_VARDIR")) {
        vardir_ = s;
    } else {
        vardir_ = rootdir_ / "var";
        setEnvVar("SPOCK_VARDIR", vardir_.string());
    }
    SAWYER_MESG(mlog[DEBUG]) <<"runtime directory (SPOCK_VARDIR): " <<vardir_ <<"\n";

    // The OPT directory holds installed packages
    if (const char *s = getenv("SPOCK_OPTDIR")) {
        optdir_ = s;
    } else {
        optdir_ = vardir_ / "installed";
        setEnvVar("SPOCK_OPTDIR", optdir_.string());
    }
    SAWYER_MESG(mlog[DEBUG]) <<"installed packages (SPOCK_OPTDIR): " <<optdir_ <<"\n";

    // The BLD directory is the temporary space for building packages
    if (const char *s = getenv("SPOCK_BLDDIR")) {
        builddir_ = s;
    } else {
        builddir_ = boost::filesystem::temp_directory_path();
        setEnvVar("SPOCK_BLDDIR", builddir_.string());
    }
    
    // All packages installed by this version of Spock depend on this version of Spock. In order to
    // achieve that, we must first create a pseudo-package to represent spock.
    scanInstalledPackages();
    Packages foundSelf = findInstalled("spock=" + std::string(VERSION));
    if (foundSelf.size() == 0) {
        std::string spockHash = randomHash();
        std::string spockSpec = "spock=" + std::string(VERSION) + "@" + spockHash;
        boost::filesystem::create_directories(optDirectory() / spockHash);
        boost::filesystem::path yamlFile = optDirectory() / (spockHash + ".yaml");
        std::ofstream yaml(yamlFile.string().c_str());
        yaml <<"package: spock\n"
             <<"version: " <<VERSION <<"\n"
             <<"timestamp: \"" <<boost::posix_time::to_simple_string(boost::posix_time::second_clock::universal_time()) <<"\"\n";
        yaml.close();
        Package::Ptr pkg = scanInstalledPackage(spockSpec);
        if (!pkg) {
            mlog[FATAL] <<"unable to create then read Spock bootstrap package\n";
            mlog[FATAL] <<"  creation attempted at " <<yamlFile <<"\n";
            mlog[FATAL] <<"  specification " <<spockSpec <<"\n";
            exit(1);
        }
        foundSelf = findInstalled("spock=" + std::string(VERSION));
        ASSERT_require(1 == foundSelf.size());
    } else if (foundSelf.size() > 1) {
        mlog[FATAL] <<"multiple installations of spock=" <<VERSION <<":";
        BOOST_FOREACH (const Package::Ptr &self, foundSelf)
            mlog[FATAL] <<" " <<self->toString();
        mlog[FATAL] <<"\n";
        exit(1);
    }
    spockItself_ = foundSelf[0];
    ASSERT_not_null(spockItself());

    // Now that we're self-aware, if there's a SPOCK_SPEC variable we can verify it's correct!
    if (const char *s = getenv("SPOCK_SPEC")) {
        if (spockItself()->toString().compare(s) != 0)
            throw Exception::Conflict("this is " + spockItself()->toString() + " but $SPOCK_SPEC is set to " + s);
    } else {
        setEnvVar("SPOCK_SPEC", spockItself_->toString());
    }

    // The SPOCK_EMPLOYED variable holds a colon-separated list of hashes of packages that are in-use. We explicitly add each
    // one to the environment stack directly because we assume that their environment variables are already initialized.
    if (const char *s = getenv("SPOCK_EMPLOYED")) {
        std::string ss = s;
        std::vector<std::string> hashes;
        boost::split(hashes, ss, boost::is_any_of(":-, \t"));
        SAWYER_MESG(mlog[DEBUG]) <<"employed packages (SPOCK_EMPLOYED): [";
        BOOST_FOREACH (std::string &hash, hashes) {
            InstalledPackage::Ptr pkg = InstalledPackage::instance(*this, hash);
            envStack_.back().packages.push_back(pkg);   // w/out updating environment variables
            SAWYER_MESG(mlog[DEBUG]) <<" " <<pkg->toString();
        }
        SAWYER_MESG(mlog[DEBUG]) <<"\n";
    } else {
        SAWYER_MESG(mlog[DEBUG]) <<"employed packages: [ ]\n";
    }

    // Make sure spock itself is always in the list of employed packages.
    insertEmployed(spockItself());

    scanGhostPackages();
}

Context::~Context() {}

boost::filesystem::path
Context::rootDirectory() const {
    return rootdir_;
}

boost::filesystem::path
Context::binDirectory() const {
    return bindir_;
}

boost::filesystem::path
Context::varDirectory() const {
    return vardir_;
}

boost::filesystem::path
Context::optDirectory() const {
    return optdir_;
}

boost::filesystem::path
Context::packageDirectory() const {
    return pkgdir_;
}

boost::filesystem::path
Context::downloadDirectory() const {
    return vardir_ / "downloads";
}

boost::filesystem::path
Context::scriptDirectory() const {
    return scriptdir_;
}

boost::filesystem::path
Context::buildDirectory() const {
    return builddir_;
}

boost::filesystem::path
Context::installedConfig(const std::string &hash) const {
    return optDirectory() / (hash + ".yaml");
}

Package::Ptr
Context::spockItself() const {
    ASSERT_not_null(spockItself_);
    return spockItself_;
}

void
Context::pushEnvironment() {
    EnvStackItem newTop;
    if (!envStack_.empty()) {
        newTop.packages = envStack_.back().packages;
        newTop.variables = envStack_.back().variables;
    }
    envStack_.push_back(newTop);
}

void
Context::popEnvironment() {
    ASSERT_forbid(envStack_.empty());
    envStack_.pop_back();
}

const Packages&
Context::employed() const {
    ASSERT_forbid(envStack_.empty());
    return envStack_.back().packages;
}

bool
Context::isEmployed(const Package::Ptr &pkg) const {
    ASSERT_not_null(pkg);
    if (!pkg->isInstalled())
        return false;
    BOOST_FOREACH (const Package::Ptr &used, employed()) {
        if (pkg->toString() == used->toString())
            return true;
    }
    return false;
}

void
Context::insertEmployed(const Package::Ptr &pkg) {
    ASSERT_not_null(pkg);
    ASSERT_always_require2(pkg->isInstalled(), pkg->toString());
    if (!isEmployed(pkg)) {
        envStack_.back().variables.prepend(asInstalled(pkg)->environmentSearchPaths());
        envStack_.back().packages.push_back(pkg);
        envStack_.back().variables.append("SPOCK_EMPLOYED", pkg->hash());
    }
}

void
Context::insertEmployed(const Packages &packages) {
    ASSERT_forbid(envStack_.empty());
    BOOST_FOREACH (const Package::Ptr &pkg, packages)
        insertEmployed(pkg);
}

void
Context::setEnvVar(const std::string &name, const std::string &value) {
    ASSERT_forbid(envStack_.empty());
    envStack_.back().variables.set(name, value);
}

Context::CommandStatus
Context::subshell(const std::vector<std::string> &command, const SubshellSettings &settings) const {
    ASSERT_forbid(envStack_.empty());
    int status = 0;
    pid_t child = fork();
    if (-1 == child) {
        throw Exception::ResourceError("fork failed: " + std::string(strerror(errno)));
    } else if (child) {
        // This is the parent process
        if (settings.output.empty()) {
            if (-1 == TEMP_FAILURE_RETRY(waitpid(child, &status, 0))) {
                throw Exception::ResourceError("wait process " + boost::lexical_cast<std::string>(child) + ": "
                                               + strerror(errno));
            }
        } else {
            Sawyer::ProgressBar<size_t> progress(mlog[MARCH], settings.progressName);
            while (1) {
                ++progress;
                if (waitpid(child, &status, WNOHANG) == child)
                    break;
                boost::this_thread::sleep_for(boost::chrono::seconds(1));
            }
        }
    } else {
        // This is the child process
        envStack_.back().variables.exportVars();
        char *arg0 = NULL;
        char **argv = NULL;
        int argc = 0;
        if (command.empty()) {
            argv = new char*[2];
            arg0 = strdup("/bin/bash");
            argv[argc++] = strdup("/bin/bash");
            argv[argc] = NULL;
        } else {
            argv = new char*[command.size()+1];
            arg0 = strdup(command[0].c_str());
            BOOST_FOREACH (const std::string &s, command)
                argv[argc++] = strdup(s.c_str());
            argv[argc] = NULL;
        }
        if (!settings.output.empty()) {
            int fd = open(settings.output.string().c_str(), O_CREAT|O_TRUNC|O_WRONLY|O_APPEND, 0666);
            status = dup2(fd, 1);
            ASSERT_require2(status != -1, strerror(errno));
            status = dup2(fd, 2);
            ASSERT_require2(status != -1, strerror(errno));
            status = close(fd);
            ASSERT_require2(status != -1, strerror(errno));
        }
        execvp(arg0, argv);
        mlog[FATAL] <<"exec failed for " <<std::string(arg0) <<": " <<std::string(strerror(errno)) <<"\n";
        exit(121);
    }

    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) == 121)
            return COMMAND_NOT_RUN;                     // our best guess
        if (WEXITSTATUS(status)==0)
            return COMMAND_SUCCESS;
    }
    return COMMAND_FAILED;
}

Context::CommandStatus
Context::subshell(const boost::filesystem::path &exe, const SubshellSettings &settings) const {
    return subshell(std::vector<std::string>(1, exe.string()), settings);
}

Package::Ptr
Context::scanInstalledPackage(const PackagePattern &spec) {
    ASSERT_forbid(spec.name().empty());
    ASSERT_forbid(spec.version().isEmpty());
    ASSERT_forbid(spec.hash().empty());
    ASSERT_require(findInstalled(spec).empty());

    Package::Ptr pkg = InstalledPackage::instance(*this, spec.hash(), optDirectory() / (spec.hash() + ".yaml"));
    ASSERT_not_null(pkg);
    allPackages_.insert(pkg);
    return pkg;
}

void
Context::scanInstalledPackages() {
    boost::filesystem::path dir = optDirectory();
    if (is_directory(dir)) {
        BOOST_FOREACH (boost::filesystem::directory_entry &dirent, boost::filesystem::directory_iterator(dir)) {
            if (is_regular_file(dirent.status()) && boost::ends_with(dirent.path().filename().string(), ".yaml")) {
                std::string hash = dirent.path().stem().string();
                if (isHash(hash)) {
                    SAWYER_MESG(mlog[DEBUG]) <<"scanning " <<dirent.path() <<"\n";
                    Package::Ptr pkg = InstalledPackage::instance(*this, hash, dirent.path());
                    allPackages_.insert(pkg);
                }
            }
        }
    }
}

void
Context::scanGhostPackages() {
    boost::filesystem::path dir = packageDirectory();
    if (is_directory(dir)) {
        BOOST_FOREACH (boost::filesystem::directory_entry &dirent, boost::filesystem::directory_iterator(dir)) {
            if (is_regular_file(dirent.status()) && boost::ends_with(dirent.path().filename().string(), ".yaml")) {
                std::string pkgName;
                try {
                    PackagePattern test = dirent.path().stem().string() + "=1"; // version added for unambiguous parsing
                    pkgName = test.name();
                } catch (const Exception::SyntaxError&) {
                    continue;
                }
                SAWYER_MESG(mlog[DEBUG]) <<"scanning " <<dirent.path() <<"\n";
                DefinedPackage::Ptr defn = DefinedPackage::instance(pkgName, dirent.path());
                definitionsByName_.insert(pkgName, defn);
                std::vector<VersionNumbers> versionSets = defn->versionsByDependency();
                BOOST_FOREACH (const VersionNumbers &vset, versionSets) {
                    GhostPackage::Ptr pkg = GhostPackage::instance(defn, vset);
                    allPackages_.insert(pkg);

                    Packages parasites = pkg->parasites();
                    allPackages_.insert(parasites);
                }
            }
        }
    }
}

Packages
Context::findInstalled(const PackagePattern &pattern) const {
    return allPackages_.find(pattern, Directory::installedP);
}

Packages
Context::findGhosts(const PackagePattern &pattern) const {
    ASSERT_require2(pattern.hash().empty(), pattern.toString());
    return allPackages_.find(pattern, Directory::notInstalledP);
}

Packages
Context::findPackages(const PackagePattern &pattern) const {
    return allPackages_.find(pattern, Directory::anyP);
}

DefinedPackage::Ptr
Context::findDefined(const PackagePattern &pattern) {
    ASSERT_forbid(pattern.name().empty());
    return definitionsByName_.getOrDefault(pattern.name());
}

void
Context::deregister(const Package::Ptr &pkg) {
    ASSERT_not_null(pkg);
    allPackages_.erase(pkg);
    if (!pkg->isInstalled())
        definitionsByName_.erase(pkg->name());
}

Packages
Context::packageDependencies(const PackagePtr &pkg) const {
    ASSERT_not_null(pkg);
    Packages retval;
    BOOST_FOREACH (const PackagePattern &pattern, pkg->dependencyPatterns()) {
        Packages deps = findPackages(pattern);
        retval.insert(retval.end(), deps.begin(), deps.end());
    }
    return retval;
}

Context::Lattice
Context::dependencyLattice(const Packages &packages) {
    Lattice lattice;
    BOOST_FOREACH (const Package::Ptr &pkg, packages) {
        lattice.insertVertexMaybe(pkg->toString());
        BOOST_FOREACH (const PackagePattern &depPat, pkg->dependencyPatterns()) {
            // Don't add the dependency patterns directly -- instead, try to match them against other packages in this supplied
            // list of packages.
            BOOST_FOREACH (const Package::Ptr &dep, packages) {
                if (depPat.matches(dep))
                    lattice.insertEdgeWithVertices(pkg->toString(), dep->toString());
            }
        }
    }
    ASSERT_forbid(Sawyer::Container::Algorithm::graphContainsCycle(lattice));
    Sawyer::Container::Algorithm::graphEraseParallelEdges(lattice);
    return lattice;
}

std::string
Context::toGraphViz(const Lattice &lattice) {
    std::ostringstream out;
    out <<"digraph dependencies {\n";
    BOOST_FOREACH (const Lattice::Vertex &v, lattice.vertices())
        out <<v.id() <<" [ label=\"" <<v.value() <<"\" ]\n";
    BOOST_FOREACH (const Lattice::Edge &e, lattice.edges())
        out <<e.source()->id() <<" -> " <<e.target()->id() <<"\n";
    out <<"}\n";
    return out.str();
}

struct SortByDependencyLattice {
    const Sawyer::Container::Map<std::string, size_t> &packageOrder;

    SortByDependencyLattice(const Sawyer::Container::Map<std::string, size_t> &packageOrder)
        : packageOrder(packageOrder) {}

    bool operator()(const Package::Ptr &a, const Package::Ptr &b) {
        return packageOrder[b->toString()] < packageOrder[a->toString()];
    }
};

void
Context::sortByDependencyLattice(Packages &packages) {
    // Build a lattice with edges from each package to those on which it depends (using only package names).
    Lattice lattice = dependencyLattice(packages);

    // Find vertices that have no predecessors
    std::list<Lattice::ConstVertexIterator> heads;
    BOOST_FOREACH (const Lattice::Vertex &vertex, lattice.vertices()) {
        if (vertex.nInEdges() == 0)
            heads.push_back(lattice.findVertex(vertex.id()));
    }

    // Create a new list by removing heads until nothing is left
    typedef Sawyer::Container::Map<std::string, size_t> PackageOrder;
    PackageOrder packageOrder;
    while (!heads.empty()) {
        Lattice::ConstVertexIterator head = heads.front();
        heads.pop_front();
        ASSERT_forbid(packageOrder.exists(head->value()));
        packageOrder.insert(head->value(), packageOrder.size());
        BOOST_FOREACH (const Lattice::Edge &edge, head->outEdges()) {
            if (edge.target()->nInEdges() == 1)
                heads.push_front(edge.target());
        }
        lattice.eraseVertex(head);
    }
    ASSERT_require(lattice.isEmpty());

    // Create a return value and sort it by package position in the traversal
    SortByDependencyLattice sorter(packageOrder);
    std::sort(packages.begin(), packages.end(), sorter);
}

} // namespace
