#include <Spock/InstalledPackage.h>

#include <Spock/Exception.h>
#include <Spock/GlobalFlag.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <yaml-cpp/yaml.h>

namespace Spock {

InstalledPackage::InstalledPackage() {}

InstalledPackage::~InstalledPackage() {}

InstalledPackage::Ptr
InstalledPackage::instance() {
    return Ptr(new InstalledPackage);
}

InstalledPackage::Ptr
InstalledPackage::instance(const Context &ctx, const std::string &hash) {
    if (!isHash(hash))
        throw Exception::SyntaxError("invalid package hash \"" + hash + "\"");
    boost::filesystem::path configFile = ctx.installedConfig(hash);
#if 1 // DEBUGGING [Robb P Matzke 2017-01-13]
    std::cerr <<"ROBB: configFile = " <<configFile <<"\n";
#endif

    // File must be a YAML file
    if (!boost::filesystem::exists(configFile))
        throw Exception::NotFound("package " + hash + " does not seem to be installed");
    YAML::Node config = YAML::LoadFile(configFile.string());

    // Create the object and initialize it
    InstalledPackage *self = new InstalledPackage;
    self->hash(hash);

    if (config["package"] && config["package"].Type() == YAML::NodeType::Scalar)
        self->name(config["package"].as<std::string>());
    if (self->name().empty())
        throw Exception::SyntaxError("no package name in file " + configFile.string());

    if (config["version"] && config["version"].Type() == YAML::NodeType::Scalar)
        self->version(config["version"].as<std::string>());
    if (self->version().isEmpty())
        throw Exception::SyntaxError("no package version in file " + configFile.string());

    if (config["dependencies"]) {
        if (config["dependencies"].Type() != YAML::NodeType::Sequence)
            throw Exception::SyntaxError("malformed package dependency list in file " + configFile.string());
        std::vector<InstalledPackage::Ptr> dependencies;
        YAML::Node depList = config["dependencies"];
        for (size_t i=0; i<depList.size(); ++i) {
            std::string depHash = depList[i]["hash"].as<std::string>();
            dependencies.push_back(instance(ctx, depHash));
        }
        self->dependencies(dependencies);
    }

    if (config["environment"]) {
        if (config["environment"].Type() != YAML::NodeType::Map)
            throw Exception::SyntaxError("malformed environment map in file " + configFile.string());
        SearchPaths searchPaths;
        for (YAML::const_iterator evar=config["environment"].begin(); evar!=config["environment"].end(); ++evar) {
            std::string varName = evar->first.as<std::string>();
            std::string varVal = evar->second.as<std::string>();
            std::vector<std::string> varParts;
            boost::split(varParts, varVal, boost::is_any_of(":"));
            searchPaths.insert(varName, varParts);
        }
        self->environmentSearchPaths(searchPaths);
    }

    if (!config["timestamp"] || config["timestamp"].Type() != YAML::NodeType::Scalar)
        throw Exception::SyntaxError("missing timestamp in file " + configFile.string());
    boost::posix_time::ptime timestamp = boost::posix_time::time_from_string(config["timestamp"].as<std::string>());
    self->timestamp(timestamp);

    return Ptr(self);
}

void
InstalledPackage::hash(const std::string &s) {
    ASSERT_require(isHash(s));
    hash_ = s;
}

void
InstalledPackage::name(const std::string &s) {
    ASSERT_forbid(s.empty());
    name_ = s;
}

void
InstalledPackage::version(const VersionNumber &v) {
    ASSERT_forbid(v.isEmpty());
    version_ = v;
}

std::string
InstalledPackage::fullName() const {
    ASSERT_forbid(name().empty());
    ASSERT_forbid(version_.isEmpty());
    ASSERT_forbid(hash().empty());
    return name() + "=" + version_.toString() + "@" + hash();
}

void
InstalledPackage::dependencies(const std::vector<InstalledPackage::Ptr> &deps) {
#ifndef NDEBUG
    BOOST_FOREACH (const InstalledPackage::Ptr &dep, deps)
        ASSERT_not_null(dep);
#endif
    dependencies_ = deps;
}

void
InstalledPackage::flags(const std::vector<GlobalFlag::Ptr> &fv) {
#ifndef NDEBUG
    BOOST_FOREACH (const GlobalFlag::Ptr &f, fv)
        ASSERT_not_null(f);
#endif
    flags_ = fv;
}

void
InstalledPackage::timestamp(const boost::posix_time::ptime &ts) {
    timestamp_ = ts;
}

void
InstalledPackage::environmentSearchPaths(const SearchPaths &sps) {
    environmentSearchPaths_ = sps;
}

} // namespace
