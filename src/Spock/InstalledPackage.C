#include <Spock/InstalledPackage.h>

#include <Spock/Exception.h>
#include <Spock/GlobalFlag.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <yaml-cpp/yaml.h>

namespace bfs = boost::filesystem;

namespace Spock {

InstalledPackage::InstalledPackage() {}

InstalledPackage::~InstalledPackage() {}

InstalledPackage::Ptr
InstalledPackage::instance() {
    return Ptr(new InstalledPackage);
}

InstalledPackage::Ptr
InstalledPackage::instance(const Context &ctx, const std::string &s) {
    boost::regex re1(".*@([0-9a-f]{8})"), re2("([0-9a-f]{8})");
    boost::smatch results;
    std::string hash;
    if (boost::regex_match(s, results, re1)) {
        hash = results.str(1);
    } else if (boost::regex_match(s, results, re2)) {
        hash = results.str(1);
    } else {
        throw Exception::SyntaxError("hash required in \"" + s + "\"");
    }
    
    boost::filesystem::path configFile = ctx.installedConfig(hash);
    return instance(ctx, hash, configFile);
}

InstalledPackage::Ptr
InstalledPackage::instance(const Context &ctx, const std::string &hash, const boost::filesystem::path &configFile) {
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

    if (config["aliases"]) {
        Aliases aliases;
        if (config["aliases"].Type() == YAML::NodeType::Scalar) {
            std::string alias = config["aliases"].as<std::string>();
            if (!alias.empty())
                aliases.insert(alias);
        } else if (config["aliases"].Type() == YAML::NodeType::Sequence) {
            for (YAML::const_iterator item=config["aliases"].begin(); item!=config["aliases"].end(); ++item) {
                if (item->Type() != YAML::NodeType::Scalar)
                    throw Exception::SyntaxError("\"aliases\" sequence item is not scalar in file " + configFile.string());
                std::string alias = item->as<std::string>();
                if (!alias.empty())
                    aliases.insert(alias);
            }
        } else {
            throw Exception::SyntaxError("\"aliases\" should be a list or scalar in file " + configFile.string());
        }
        self->aliases(aliases);
    }
    
    if (config["dependencies"]) {
        std::vector<std::string> depNames;
        if (config["dependencies"].Type() == YAML::NodeType::Scalar) {
            depNames.push_back(config["dependencies"].as<std::string>());
        } else if (config["dependencies"].Type() == YAML::NodeType::Sequence) {
            for (YAML::const_iterator dep=config["dependencies"].begin(); dep!=config["dependencies"].end(); ++dep) {
                if (dep->Type() != YAML::NodeType::Scalar)
                    throw Exception::SyntaxError("dependency sequence item is not scalar in file " + configFile.string());
                depNames.push_back(dep->as<std::string>());
            }
        } else {
            throw Exception::SyntaxError("\"dependencies\" should be a list or scalar in file " + configFile.string());
        }

        BOOST_FOREACH (const std::string &depName, depNames) {
            PackagePattern pattern(depName);
            ASSERT_require(!pattern.name().empty());
            ASSERT_require(pattern.versionComparison() == PackagePattern::VERS_EQ);
            ASSERT_require(!pattern.version().isEmpty());
            ASSERT_require(isHash(pattern.hash()));
            self->dependencyPatterns_.push_back(pattern);
        }

    } else if (config["depends"]) {
        throw Exception::SyntaxError("use \"dependencies\" instead of \"depends\" in file " + configFile.string());
    }
    
    if (config["environment"]) {
        if (config["environment"].Type() != YAML::NodeType::Map)
            throw Exception::SyntaxError("malformed environment map in file " + configFile.string());
        Environment searchPaths;
        for (YAML::const_iterator evar=config["environment"].begin(); evar!=config["environment"].end(); ++evar) {
            std::string varName = evar->first.as<std::string>();
            std::string varVal = evar->second.as<std::string>();
            searchPaths.set(varName, varVal);
        }
        self->environmentSearchPaths(searchPaths);
    }

    if (!config["timestamp"] || config["timestamp"].Type() != YAML::NodeType::Scalar)
        throw Exception::SyntaxError("missing timestamp in file " + configFile.string());
    boost::posix_time::ptime timestamp = boost::posix_time::time_from_string(config["timestamp"].as<std::string>());
    self->timestamp(timestamp);

    return Ptr(self);
}

VersionNumber
InstalledPackage::version() const {
    return version_;
}

void
InstalledPackage::version(const VersionNumber &v) {
    version_ = v;
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
InstalledPackage::environmentSearchPaths(const Environment &sps) {
    environmentSearchPaths_ = sps;
}

void
InstalledPackage::remove(Context &ctx) {
    bfs::path yamlFile = ctx.installedConfig(hash());
    if (bfs::exists(yamlFile)) {                        // might have been removed already
        bfs::remove(yamlFile);
        bfs::path prefix = ctx.optDirectory() / hash();
        bfs::remove_all(prefix);
    }
}

} // namespace
