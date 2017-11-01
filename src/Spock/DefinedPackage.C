#include <Spock/DefinedPackage.h>

#include <Spock/TemporaryDirectory.h>
#include <Spock/Exception.h>
#include <Spock/InstalledPackage.h>
#include <Spock/PackageLists.h>
#include <Spock/PackagePattern.h>
#include <Spock/Solver.h>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace bfs = boost::filesystem;
using namespace Sawyer::Message::Common;

namespace Spock {

Sawyer::Message::Facility DefinedPackage::mlog;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                      Supporting functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum Required { OPTIONAL_NODE, REQUIRED_NODE };

// Location for a node, error, etc.
struct Location {
    boost::filesystem::path fileName;
    std::string nodePath;
    YAML::Node node;

    Location(const boost::filesystem::path fileName, const std::string &nodePath, YAML::Node node = YAML::Node())
        : fileName(fileName), nodePath(nodePath), node(node) {}

#if 0 // [Robb P Matzke 2017-01-30]
private:
    Location(const Location&) /*delete*/ { ASSERT_not_reachable("no copy constructor"); }
    Location& operator=(const Location&) /*delete*/ { ASSERT_not_reachable("no assignment operator"); }
#endif
};

// Turn something into a string
template<class T>
std::string
toString(const T &t) {
    return boost::lexical_cast<std::string>(t);
}

// Throw an exception that includes information about where the problem occured
template<class Exception>
static void
fail(const Location &loc, std::string mesg) __attribute__((noreturn));

template<class Exception>
static void
fail(const Location &loc, std::string mesg) {
    if (!loc.nodePath.empty())
        mesg += std::string(mesg.empty()?"":" ") + "at " + loc.nodePath;
    if (!loc.fileName.empty())
        mesg += std::string(mesg.empty()?"":" ") + "in " + loc.fileName.string();
    throw Exception(mesg);
}

template<class Exception>
static void
fail(const DefinedPackage *pkg, std::string mesg, const bfs::path &output = bfs::path()) __attribute__((noreturn));

template<class Exception>
static void
fail(const DefinedPackage *pkg, std::string mesg, const bfs::path &output) {
    mesg += " for " + pkg->name();
    if (!output.empty())
        mesg += "; see " + output.string() + " for details";
    throw Exception(mesg);
}

// For debugging: prints info about a YAML node since YAML-CCP's documentation consists of just example code with no
// descriptions of edge cases.
std::string
debugNodeType(const YAML::Node &n) {
    if (n) {
        switch (n.Type()) {
            case YAML::NodeType::Null:
                ASSERT_require(n.IsNull());
                return "null";
            case YAML::NodeType::Scalar:
                ASSERT_require(n.IsScalar());
                return "scalar";
            case YAML::NodeType::Sequence:
                ASSERT_require(n.IsSequence());
                return "sequence";
            case YAML::NodeType::Map:
                ASSERT_require(n.IsMap());
                return "map";
            case YAML::NodeType::Undefined:
                //ASSERT_require(n.IsUndefined()); -- not present in yamlcpp-0.5.3
                return "undefined";
            default:
                ASSERT_not_reachable("unknown YAML node type");
        }
    } else {
        return "false";
    }
}

// Read a single scalar value for a node. The dummy arg is only to provide a default return type since c++03 doesn't allow
// default template arguments for function templates.
template<class T>
T
readScalar(const Location &loc, Required req = REQUIRED_NODE) {
    if (!loc.node || loc.node.IsNull()) {
        if (REQUIRED_NODE == req)
            fail<Exception::SyntaxError>(loc, "missing property");
        return T("");
    } else if (loc.node.IsScalar()) { 
        return T(loc.node.as<std::string>());
    } else {
        fail<Exception::SyntaxError>(loc, "scalar expected");
    }
}

// Reads a node that has either a scalar value or a sequence of scalars, and return those values as a vector of T.
template<class T>
std::vector<T>
readListOfScalars(const Location &loc, Required req = REQUIRED_NODE) {
    std::vector<T> retval;
    if (!loc.node || loc.node.IsNull()) {
        if (REQUIRED_NODE == req)
            fail<Exception::SyntaxError>(loc, "missing property");
    } else if (loc.node.IsScalar()) {
        retval.push_back(T(loc.node.as<std::string>()));
    } else if (loc.node.IsSequence()) {
        size_t i = 0;
        BOOST_FOREACH (const YAML::Node item, loc.node) {
            if (!item.IsScalar())
                fail<Exception::SyntaxError>(loc, "scalar expected for item #" + toString(i));
            retval.push_back(T(item.as<std::string>()));
            ++i;
        }
    } else {
        fail<Exception::SyntaxError>(loc, "malformed property");
    }
    return retval;
}

// Reads all objects matching a specified version number and returns their nodes in the order they appear in the file.
std::vector<YAML::Node>
readVersionedNodes(const Location &loc, const VersionNumber &requestedVersion, Required req = REQUIRED_NODE) {
    std::vector<YAML::Node> retval;

    if (!loc.node || loc.node.IsNull()) {
        if (REQUIRED_NODE == req)
            fail<Exception::SyntaxError>(loc, "missing property");
        return retval;
    } else if (loc.node.Type() == YAML::NodeType::Sequence) {
        size_t itemNum = 0;
        BOOST_FOREACH (const YAML::Node item, loc.node) {
            if (item["version"].IsNull())
                fail<Exception::SyntaxError>(loc, "missing version for item #" + toString(itemNum));
            if (item["version"].Type() != YAML::NodeType::Scalar)
                fail<Exception::SyntaxError>(loc, "version must be scalar for item #" + toString(itemNum));
            PackagePattern pattern = item["version"].as<std::string>();
            if (pattern.matches(requestedVersion))
                retval.push_back(item);
            ++itemNum;
        }
    } else {
        fail<Exception::SyntaxError>(loc, "expected list");
    }

    if (retval.empty() && REQUIRED_NODE == req)
        fail<Exception::NotFound>(loc, "no match for " + requestedVersion.toString());

    return retval;
}


// Reads a versioned node and returns its location.  For instance, config_["download"][VERSION]["shell"] means scan the
// "download" list to find the *last* item matching the specified version number, then return the node for the "shell"
// property.  The new location is the same as the old, except the node path is extended with .VERSION.shell., where VERSION
// is the string from the YAML file (e.g., ">=5.2") rather than the requested version (since that's already known to the
// caller).
//
// If SUBNODE is the empty string, then this just returns the last object with the specified version, in which case the path
// will be extended with just ".VERSION".
//
// If no match is found and none is required, the return value is a null node and the path is not extended.
Location
readVersionedNode(const Location &loc, const VersionNumber &requestedVersion, const std::string &subnode,
                  Required req = REQUIRED_NODE) {
    std::vector<YAML::Node> items = readVersionedNodes(loc, requestedVersion, req);
    if (items.empty())
        return Location(loc.fileName, loc.nodePath, YAML::Node());
    YAML::Node found = items.back();

    ASSERT_require(found["version"].IsScalar());
    std::string path = found["version"].as<std::string>();


    YAML::Node retval = subnode.empty() ? found : found[subnode];
    if (!subnode.empty())
        path += "." + subnode;

    if ((!retval || retval.IsNull()) && REQUIRED_NODE == req)
        fail<Exception::SyntaxError>(loc, "no match for " + requestedVersion.toString() + (subnode.empty()?"":"."+subnode));
    return Location(loc.fileName, loc.nodePath + "." + path, retval);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                      DefinedPackage members
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DefinedPackage::DefinedPackage(const std::string &pkgName, const bfs::path &configFile)
    : name_(pkgName), configFile_(configFile) {
    try {
        config_ = YAML::LoadFile(configFile.string());
    } catch (const YAML::ParserException &e) {
        throw YAML::ParserException(e.mark, e.msg + " in " + configFile.string());
    }

    // If there's a "package" property then it must be a scalar that matches pkgName
    Location pkgNode(configFile, "package", config_["package"]);
    std::string advertisedName = readScalar<std::string>(pkgNode, OPTIONAL_NODE);
    if (!advertisedName.empty() && advertisedName != name_)
        fail<Exception::Conflict>(pkgNode, "value conflicts with file name");

    // There must be a "versions" property that lists version numbers
    Location versionsNode(configFile, "versions", config_["versions"]);
    std::vector<VersionNumber> vv = readListOfScalars<VersionNumber>(versionsNode);
    if (vv.empty())
        fail<Exception::SyntaxError>(versionsNode, "cannot be empty");
    BOOST_FOREACH (const VersionNumber &v, vv)
        versions_.insert(v);
}

DefinedPackage::~DefinedPackage() {}

DefinedPackage::Ptr
DefinedPackage::instance(const std::string &pkgName, const bfs::path &configFile) {
    return Ptr(new DefinedPackage(pkgName, configFile));
}

std::vector<VersionNumbers>
DefinedPackage::versionsByDependency() const {
    std::vector<VersionNumbers> retval;
    std::vector<VersionNumber> versions(versions_.values().begin(), versions_.values().end());

    Location dependenciesNode(configFile_, "dependencies", config_["dependencies"]);
    if (!dependenciesNode.node)
        fail<Exception::SyntaxError>(dependenciesNode, "missing property");
    if (!dependenciesNode.node.IsSequence())
        fail<Exception::SyntaxError>(dependenciesNode, "list expected");

    size_t itemNum = 0;
    BOOST_FOREACH (YAML::Node item, dependenciesNode.node) {
        if (versions.empty())
            break;
        if (!item["version"].IsScalar())
            fail<Exception::SyntaxError>(dependenciesNode, "expected scalar version property in item #" + toString(itemNum));
        PackagePattern pattern = item["version"].as<std::string>();

        retval.push_back(VersionNumbers());
        for (size_t i=0; i<versions.size(); /*void*/) {
            if (pattern.matches(versions[i])) {
                retval.back().insert(versions[i]);
                versions.erase(versions.begin() + i);
            } else {
                ++i;
            }
        }

        if (retval.back().isEmpty())
            retval.pop_back();
        ++itemNum;
    }

    if (!versions.empty() && mlog[WARN]) {
        mlog[WARN] <<"the following versions of " <<name() <<" are missing dependency information in " + configFile_.string();
        BOOST_FOREACH (const VersionNumber &v, versions)
            mlog[WARN] <<" " <<v.toString();
        mlog[WARN] <<"\n";
    }

    return retval;
}

bool
DefinedPackage::isSupportedVersion(const VersionNumber &vers) const {
    BOOST_FOREACH (const VersionNumber &v, versions_.values()) {
        if (vers == v)
            return true;
    }
    return false;
}

std::vector<PackagePattern>
DefinedPackage::dependencyPatterns(const VersionNumber &vers) {
    Location loc = readVersionedNode(Location(configFile_, "dependencies", config_["dependencies"]), vers, "install");
    std::vector<PackagePattern> retval = readListOfScalars<PackagePattern>(loc);
    if (retval.empty())
        retval.push_back("spock=" + std::string(VERSION));
    return retval;
}

std::vector<PackagePattern>
DefinedPackage::parasitePatterns(const VersionNumber &vers, std::vector<Aliases> &aliases /*out*/) const {
    Location loc = readVersionedNode(Location(configFile_, "post-install", config_["post-install"]),
                                              vers, "parasites", OPTIONAL_NODE);
    std::vector<std::string> lines = readListOfScalars<std::string>(loc, OPTIONAL_NODE);

    aliases.clear();
    std::vector<PackagePattern> parasites;
    BOOST_FOREACH (std::string &line, lines) {
        boost::trim(line);
        std::vector<std::string> words;
        boost::split(words, line, boost::is_any_of(" "), boost::token_compress_on);
        ASSERT_forbid(words.empty());
        PackagePattern pattern(words[0]);
        
        if (pattern.name().empty())
            fail<Exception::SyntaxError>(loc, "parasite \"" + pattern.toString() + "\" needs a name");
        if (pattern.name() == name())
            fail<Exception::SyntaxError>(loc, "parasite \"" + pattern.toString() + "\" cannot have same name as its host");
        if (!pattern.version().isEmpty() &&
            pattern.versionComparison() != PackagePattern::VERS_EQ &&
            pattern.versionComparison() != PackagePattern::VERS_HY) // alias of '=' just for convenience
            fail<Exception::SyntaxError>(loc, "parasite \"" + pattern.toString() + "\" must use '=' version");

        parasites.push_back(pattern);

        Aliases a;
        for (size_t i=1; i<words.size(); ++i)
            a.insert(words[i]);
        aliases.push_back(a);
    }
    return parasites;
}

bfs::path
DefinedPackage::cachedDownloadFile(Context &ctx, const Settings &settings) const {
    bfs::create_directories(ctx.downloadDirectory());
    return ctx.downloadDirectory() / (name_ + "-" + settings.version.toString() + ".tar.gz");
}

std::string
DefinedPackage::findCommands(const std::string &sectionName, const VersionNumber &version) {
    Location loc = readVersionedNode(Location(configFile_, sectionName, config_[sectionName]), version, "shell");
    return readScalar<std::string>(loc);
}

std::vector<std::string>
DefinedPackage::shellVariables(const Settings &settings) const {
    std::vector<std::string> retval;

    Location variablesNode(configFile_, "variables", config_["variables"]);
    std::vector<YAML::Node> matchingObjects = readVersionedNodes(variablesNode, settings.version, OPTIONAL_NODE);
    BOOST_FOREACH (YAML::Node object, matchingObjects) {
        ASSERT_require(object.IsMap());
        for (YAML::const_iterator pair = object.begin(); pair != object.end(); ++pair) {
            std::string varName = pair->first.as<std::string>();
            if (varName != "version") {             // "version" is the section selection criterium, not a variable
                if (!pair->second.IsScalar()) {
                    Location errorLoc(configFile_, "variables." + object["version"].as<std::string>() + "." + varName);
                    fail<Exception::SyntaxError>(errorLoc, "scalar variable value expected");
                }
                std::string value = pair->second.as<std::string>();
                retval.push_back(varName + "=\"" + value + "\"");
            }
        }
    }
    return retval;
}

bfs::path
DefinedPackage::createShellScript(const Settings &settings, const bfs::path &cwd, const std::string &commands,
                                  const std::vector<std::string> &extraVars) const {
    ASSERT_require(bfs::is_directory(cwd));
    bfs::path scriptName = cwd / bfs::unique_path();

    std::string name_uc = boost::to_upper_copy(name());
    BOOST_FOREACH (char &ch, name_uc) {
        if (!isalnum(ch) && ch!='_')
            ch = '_';
    }

    // Write to string first for easier debugging
    std::ostringstream ss;
    ss <<"#!/bin/bash\n"
       <<"# Script for Spock's " <<name_ <<"=" <<settings.version.toString() <<" package\n"
       <<"# This is a generated file -- do not edit.\n"
       <<"\n"
       <<"cd '" <<cwd.string() <<"'\n"
       <<"\n"
       <<"# --- Spock boilerplate ---\n"
       <<"export\n"                                     // show environment for debugging
       <<"PACKAGE_NAME='" <<name() <<"'\n"
       <<"PACKAGE_NAME_UC='" <<name_uc <<"'\n"
       <<"PACKAGE_VERSION='" <<settings.version.toString() <<"'\n"
       <<"PACKAGE_HASH='" <<settings.hash <<"'\n"
       <<"PACKAGE_SPEC='" <<name() <<"=" <<settings.version.toString() <<"@" <<settings.hash <<"'\n";
    BOOST_FOREACH (const std::string &var, extraVars)
        ss <<var <<"\n";
    ss <<"source $SPOCK_SCRIPTS/impl/installation-support.sh || exit 1\n"
       <<"\n"
       <<"# --- Package variables ---\n";
    BOOST_FOREACH (const std::string &var, shellVariables(settings))
        ss <<var <<"\n";
    ss <<"\n"
       <<"# --- Package script ---\n"
       <<"echo \"cwd is $(pwd)\"\n"
       <<"set -ex\n"
       <<commands
       <<"\n"
       <<"# --- Spock finalization ---\n"
       <<"spock-finalize\n";

    // Debugging
    if (mlog[DEBUG]) {
        mlog[DEBUG] <<"shell script " <<scriptName <<":\n";
        std::vector<std::string> lines;
        std::string s = ss.str();
        boost::split(lines, s, boost::is_any_of("\n"));
        for (size_t i=0; i<lines.size(); ++i)
            mlog[DEBUG] <<"  " <<std::setw(3) <<std::right <<(i+1) <<"|" <<lines[i] <<"\n";
    }

    // Save script to executable file
    std::ofstream script(scriptName.string().c_str());
    ASSERT_require(script.is_open());
    script <<ss.str();
    script.close();
    ASSERT_require(script.good());
    bfs::permissions(scriptName, bfs::owner_all);

    return scriptName;
}

bfs::path
DefinedPackage::download(Context &ctx, const Settings &settings) {
    // Did we already download this file?
    bfs::path dest = cachedDownloadFile(ctx, settings);
    if (!bfs::exists(dest)) {
        mlog[INFO] <<"downloading " <<name() <<"=" <<settings.version.toString() <<"\n";
        std::vector<std::string> extraVars;
        extraVars.push_back("PACKAGE_ACTION=download");

        // Run the download script
        std::string downloadCommands = findCommands("download", settings.version);
        TemporaryDirectory workingDir(ctx.buildDirectory() / bfs::unique_path("spock-download-%%%%%%%%"));
        if (settings.keepTempFiles)
            workingDir.keep();
        bfs::path script = createShellScript(settings, workingDir.path(), downloadCommands, extraVars);
        Context::SubshellSettings ssSettings("downloading " + name() + "=" + settings.version.toString());
        if (settings.quiet)
            ssSettings.output = ctx.downloadDirectory() / (name_ + "-" + settings.version.toString() + "-download-log.txt");
        if (ctx.subshell(script, ssSettings) != Context::COMMAND_SUCCESS)
            fail<Exception::CommandError>(this, "download failed", ssSettings.output);
        
        // Copy the "download.tar.gz" file to the download cache and clean up
        if (!bfs::exists(workingDir.path()/"download.tar.gz"))
            fail<Exception::CommandError>(this, "download script did not create download.tar.gz", ssSettings.output);
        boost::system::error_code ec;
        bfs::copy_file(workingDir.path()/"download.tar.gz", dest, ec); // move won't work across filesystems, so copy
        if (ec.value() != 0)
            fail<Exception::CommandError>(this, "cannot copy download.tar.gz to " + dest.string());
    }
    ASSERT_require(bfs::exists(dest));
    return dest;
}

std::string
DefinedPackage::mySpec(const Settings &settings) const {
    std::string s = name();
    if (!settings.version.isEmpty())
        s += "=" + settings.version.toString();
    if (!settings.hash.empty())
        s += "@" + settings.hash;
    return s;
}

// Type is either "build" or "install"
Packages
DefinedPackage::solveDependencies(Context &ctx, const Settings &settings, const std::string &type1, const std::string &type2) {
    std::string typesStr = type1;
    Location dependenciesNode(configFile_, "dependencies", config_["dependencies"]);
    Location depNode1 = readVersionedNode(dependenciesNode, settings.version, type1);
    std::vector<PackagePattern> depNames = readListOfScalars<PackagePattern>(depNode1);

    if (!type2.empty()) {
        typesStr += "+" + type2;
        Location depNode2 = readVersionedNode(dependenciesNode, settings.version, type2);
        std::vector<PackagePattern> v = readListOfScalars<PackagePattern>(depNode2);
        depNames.insert(depNames.end(), v.begin(), v.end());
    }

    Solver solver(ctx);
    solver.onlyInstalled(true);
    solver.solve(depNames);
    if (solver.nSolutions()==0) {
        if (mlog[ERROR]) {
            mlog[ERROR] <<"could not satisfy " <<typesStr <<" dependencies for " <<mySpec(settings) <<":";
            BOOST_FOREACH (const PackagePattern &pat, depNames)
                mlog[ERROR] <<" " <<pat.toString();
            mlog[ERROR] <<"\n";
            BOOST_FOREACH (const std::string &mesg, solver.messages().values())
                mlog[ERROR] <<"    " <<mesg <<"\n";
        }
        fail<Exception::NotFound>(this, "could not satisfy " + typesStr + " dependencies");
    }
    return solver.solution(0);
}

void
DefinedPackage::installConfigFile(Context &ctx, const Settings &settings, const Packages &installDeps,
                                  const bfs::path &yamlSrc, const bfs::path &installDir) {
    std::ofstream yaml(yamlSrc.string().c_str(), std::ios::app);
    yaml <<"\n"
         <<"package: '" <<name() <<"'\n"
         <<"version: '" <<settings.version.toString() <<"'\n"
         <<"timestamp: \"" <<boost::posix_time::to_simple_string(boost::posix_time::second_clock::universal_time()) <<"\"\n";

    // Secondary names (aliases)
    Location aliasesNode = readVersionedNode(Location(configFile_, "dependencies", config_["dependencies"]),
                                             settings.version, "aliases", OPTIONAL_NODE);
    std::vector<std::string> aliases = readListOfScalars<std::string>(aliasesNode, OPTIONAL_NODE);
    if (!aliases.empty()) {
        yaml <<"\naliases:\n";
        BOOST_FOREACH (const std::string &alias, aliases)
            yaml <<"  - '" <<alias <<"'\n";
    }

    // Dependencies. We want only the actual installation dependencies of this package, not all the packages the solver brought
    // into scope or which might have already been employed by the user. In order to get that, we match each of the new
    // package's dependency patterns against the full list of dependencies and save only those which match.
    std::set<std::string> depStrs;
    depStrs.insert(ctx.spockItself()->toString());
    BOOST_FOREACH (const PackagePattern &depPat, dependencyPatterns(settings.version)) {
        BOOST_FOREACH (const Package::Ptr &pkg, installDeps) {
            if (depPat.matches(pkg)) {
                depStrs.insert(pkg->toString());
                break;
            }
        }
    }
    yaml <<"\ndependencies:\n";
    BOOST_FOREACH (const std::string &s, depStrs)
        yaml <<"  - '" <<s <<"'\n";
    
    yaml.close();
    bfs::path yamlDst = installDir / (settings.hash + ".yaml");
    bfs::copy_file(yamlSrc, yamlDst);
}

std::string
DefinedPackage::configHash(Context &ctx, const Settings &settings, const Packages &installDeps, const Packages &buildDeps) {
    // Create a hash from all things that affect the configuration.
    bfs::path tmpFile = bfs::temp_directory_path() / bfs::unique_path("spock-%%%%%%%%");
    bfs::copy_file(configFile_, tmpFile);
    {
        std::ofstream f(tmpFile.string().c_str(), std::ios::app);
        BOOST_FOREACH (const Package::Ptr &pkg, installDeps)
            f <<pkg->toString() <<"\n";
        BOOST_FOREACH (const Package::Ptr &pkg, buildDeps)
            f <<pkg->toString() <<"\n";
        f <<name() <<"=" <<settings.version.toString() <<"\n";
    }

    std::string hash;
    if (FILE *p = popen(("sha1sum " + tmpFile.string()).c_str(), "r")) {
        char buf[64];
        if (fgets(buf, sizeof buf, p) && strlen(buf)>8)
            hash = std::string(buf).substr(0, 8);
        (void) pclose(p);
    }

    return hash;
}

Package::Ptr
DefinedPackage::install(Context &ctx, Settings &settings /*in,out*/) {
    Context::SavedStack saved(ctx);                      // for exception safety
    ctx.pushEnvironment();

    ASSERT_require2(settings.hash.empty(), mySpec(settings) + " appears to have been installed already (or attempted)");
    bfs::path tarball = download(ctx, settings);

    settings.hash = bfs::unique_path("%%%%%%%%").string();
    ASSERT_require(isHash(settings.hash));
    mlog[INFO] <<"building " <<mySpec(settings) <<"\n";

    // Can install dependencies be satisfied? There's no point taking time to compile a package if we can't create its
    // installation YAML file due to not being able to meet all its install dependencies.
    mlog[DEBUG] <<"solving " <<mySpec(settings) <<" installation dependencies...\n";
    Packages installDeps = solveDependencies(ctx, settings, "install");

    // Can build dependencies be satisfied?  These are the ones we're going to use to build the package, so they better be
    // installed already.
    mlog[DEBUG] <<"solving " <<mySpec(settings) <<" build dependencies...\n";
    Packages buildDeps = solveDependencies(ctx, settings, "install", "build");
    mlog[DEBUG] <<"using " <<mySpec(settings) <<" build dependencies\n";
    Context::SavedStack contextExcursion(ctx);
    ctx.pushEnvironment();                              // contextExcursion's destructor will pop this context
    ctx.insertEmployed(buildDeps);
    if (mlog[INFO]) {
        BOOST_FOREACH (const Package::Ptr &p, buildDeps)
            mlog[INFO] <<"  using " <<p->toString() <<"\n";
    }

    // Create directories.  The installation prefix is temporary for now so it gets deleted if there's an error.
    bfs::path installDir = settings.installDirOverride.empty() ? ctx.optDirectory() : settings.installDirOverride;
    TemporaryDirectory installationPrefix(installDir / settings.hash);
    bfs::path pkgRoot = installationPrefix.path() / name();
    bfs::create_directory(pkgRoot);
    TemporaryDirectory workingDir(ctx.buildDirectory() / bfs::unique_path("spock-build-%%%%%%%%"));
    if (settings.keepTempFiles) {
        installationPrefix.keep();
        workingDir.keep();
    }
    
    // Create the installation shell script
    std::string installCommands = findCommands("install", settings.version);
    std::vector<std::string> extraVars;
    extraVars.push_back("PACKAGE_ACTION=install");
    extraVars.push_back("PACKAGE_ROOT='" + pkgRoot.string() + "'");
    bfs::path script = createShellScript(settings, workingDir.path(),
                                         "tar xf '" + tarball.string() + "'\n\n" +
                                         "spock-apply-patches $patches\n\n" +
                                         installCommands,
                                         extraVars);

    // If we've previously attempted and failed to install this exact configuration, don't bother wasting time doing it again.
    bfs::path attempted;
    std::string confhash = configHash(ctx, settings, installDeps, buildDeps);
    if (!confhash.empty()) {
        attempted = installDir / (confhash + "-build-log.txt");
    } else {
        attempted = installDir / (settings.hash + "-build-log.txt");
    }
    if (!settings.tryAgain && bfs::exists(attempted))
        fail<Exception::Conflict>(this, "installation was previously attempted and failed", attempted);
    
    // Run the installation script. Put the output in a place where it won't be destroyed right away if there's a
    // failure. On success, we'll move it to a permanent location for historical record.
    Context::SubshellSettings ssSettings("building " + mySpec(settings));
    if (settings.quiet)
        ssSettings.output = attempted;
    if (ctx.subshell(script, ssSettings) != Context::COMMAND_SUCCESS)
        fail<Exception::CommandError>(this, "installation script failed", ssSettings.output);

    // The script must leave an "installed.yaml" file that desribes how to use the package.  Combine that with some other info
    // about the package to create an install config. Do it in such a way that the package is installed only after its yaml
    // file is fully created.
    bfs::path yamlSrc = workingDir.path() / "installed.yaml";
    if (!bfs::exists(yamlSrc))
        fail<Exception::CommandError>(this, "\"installed.yaml\" not created", ssSettings.output);
    mlog[INFO] <<"installing " <<mySpec(settings) <<"\n";
    installConfigFile(ctx, settings, installDeps, yamlSrc, installDir);

    // Finalize installation of this package before moving on to post-install stuff
    if (!ssSettings.output.empty())
        bfs::rename(ssSettings.output, installDir / settings.hash / "build-log.txt");
    installationPrefix.keep();
    bfs::remove(attempted);
    Package::Ptr retval = ctx.scanInstalledPackage(mySpec(settings));
    contextExcursion.restore();                         // no need for the build environment anymore

    // Post-install should run in the context of the new package and its usage dependencies. This also checks that we can use
    // the package we just installed, so we set things up even if the post-install does nothing.
    ctx.pushEnvironment();                              // will be popped by the contextExcursion destructor
    ctx.insertEmployed(installDeps);
    ctx.insertEmployed(retval);
    postInstall(ctx, settings, workingDir, pkgRoot);

    return retval;
}

void
DefinedPackage::postInstall(Context &ctx, Settings &settings,
                            const TemporaryDirectory &workingDir, const bfs::path &pkgRoot) {
    if (config_["post-install"]) {
        std::string postInstallCommands = findCommands("post-install", settings.version);
        std::vector<std::string> extraVars;
        extraVars.push_back("PACKAGE_ACTION=post-install");
        extraVars.push_back("PACKAGE_ROOT='" + pkgRoot.string() + "'");
        bfs::path script = createShellScript(settings, workingDir.path(), postInstallCommands, extraVars);
        Context::SubshellSettings ssSettings("post " + name() + "=" + settings.version.toString());
        if (settings.quiet)
            ssSettings.output = pkgRoot.parent_path() / "post-install-log.txt";
        if (ctx.subshell(script, ssSettings) != Context::COMMAND_SUCCESS)
            fail<Exception::CommandError>(this, "post-install commands failed", ssSettings.output);

        // If parasites are defined, then the post-install must drop a "parasites" file that lists the full specs of the
        // installed parasites.
        std::vector<Aliases> aliases;
        std::vector<PackagePattern> pps = parasitePatterns(settings.version, aliases /*out*/);
        std::vector<bool> parasitePatternAliasesSatisfied(pps.size(), false);
        if (!pps.empty()) {

            // Read parasite names from "parasites" file and make sure Spock knows about them
            bfs::path parasitesFileName = workingDir.path() / "parasites";
            if (!bfs::exists(parasitesFileName)) {
                fail<Exception::CommandError>(this, "post-install failed to create " + parasitesFileName.string(),
                                              ssSettings.output);
            }
            std::ifstream parasitesFile(parasitesFileName.string().c_str());
            std::string line;
            while (std::getline(parasitesFile, line).good()) {
                boost::trim(line);
                if (line.empty() || '#'==line[0])
                    continue;
                PackagePattern pattern(line);
                if (pattern.hash().empty()) {
                    fail<Exception::CommandError>(this, "incomplete spec '" + pattern.toString() + "'"
                                                  " in " + parasitesFileName.string());
                }
                Package::Ptr installed = ctx.scanInstalledPackage(pattern);
                settings.parasites.push_back(installed);

                // The parasite we just installed should have a matching pattern listed in the definition file, and the aliases
                // for each definition pattern must exactly match one of the packages that was installed for it.
                bool found = false;
                for (size_t i=0; i<pps.size(); ++i) {
                    const PackagePattern &p = pps[i];
                    if (p.matches(installed)) {
                        found = true;
                        Aliases installedAliases = installed->aliases();
                        if (installedAliases == aliases[i])
                            parasitePatternAliasesSatisfied[i] = true;
                        break;
                    }
                }
                if (!found) {
                    mlog[WARN] <<mySpec(settings) <<" installed " <<installed->toString() <<" but does not have a "
                               <<"matching parasite pattern at post-install.parasites in " <<configFile_ <<"\n";
                }
            }
            parasitesFile.close();

            // All the parasites defined in the file should have been installed
            for (size_t i=0; i<pps.size(); ++i) {
                const PackagePattern &pattern = pps[i];
                bool found = false;
                BOOST_FOREACH (const Package::Ptr &installed, settings.parasites) {
                    if (pattern.matches(installed)) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    mlog[ERROR] <<mySpec(settings) <<" failed to install parasite " <<pattern.toString() <<"\n";
                    parasitePatternAliasesSatisfied[i] = true; // to prevent more diagnostics below
                }
            }

            // Report warnings for any parasite pattern that matches one or more installed packages (otherwise an error above)
            // but none of those packages has the exact same set of aliases as listed for the parasite pattern in the config
            // file.
            ASSERT_require(pps.size() == parasitePatternAliasesSatisfied.size());
            for (size_t i=0; i<pps.size(); ++i) {
                if (!parasitePatternAliasesSatisfied[i]) {
                    mlog[WARN] <<mySpec(settings) <<" parasite pattern " <<pps[i].toString()
                               <<" aliases";
                    BOOST_FOREACH (const std::string &s, aliases[i].values())
                        mlog[WARN] <<" " <<s;
                    mlog[WARN] <<" are not exactly matched by any installed parasite:\n";
                    BOOST_FOREACH (const Package::Ptr &installedParasite, settings.parasites) {
                        if (pps[i].matches(installedParasite)) {
                            mlog[WARN] <<"  " <<installedParasite->toString() <<":";
                            Aliases installedAliases = installedParasite->aliases();
                            BOOST_FOREACH (const std::string &alias, installedAliases.values())
                                mlog[WARN] <<" " <<alias;
                            mlog[WARN] <<"\n";
                        }
                    }
                }
            }
        }
    }
}

} // namespace
