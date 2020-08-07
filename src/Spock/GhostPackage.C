#include <Spock/GhostPackage.h>

#include <Spock/DefinedPackage.h>
#include <Spock/PackagePattern.h>

namespace Spock {

GhostPackage::GhostPackage() {}

GhostPackage::GhostPackage(const DefinedPackage::Ptr &defn, const VersionNumbers &versions)
    : defn_(defn), versions_(versions) {
    ASSERT_not_null(defn);
    ASSERT_forbid(versions.isEmpty());
#ifndef NDEBUG
    BOOST_FOREACH (const VersionNumber &v, versions.values())
        ASSERT_require(defn->isSupportedVersion(v));
#endif
    name(defn->name());
}

GhostPackage::~GhostPackage() {}

// class method
GhostPackage::Ptr
GhostPackage::instance(const DefinedPackage::Ptr &defn, const VersionNumbers &versions) {
    return Ptr(new GhostPackage(defn, versions));
}

// class method
GhostPackage::Ptr
GhostPackage::instance(const GhostPackage::Ptr &orig, const VersionNumbers &newVersions) {
    ASSERT_not_null(orig);
    ASSERT_forbid(newVersions.isEmpty());
    ASSERT_require(orig->versions().existsAll(newVersions));
    Ptr self(new GhostPackage(orig->definition(), newVersions));
    self->name(orig->name());
    self->aliases(orig->aliases());
    ASSERT_require(orig->isParasite() == self->isParasite());
    return self;
}

VersionNumber
GhostPackage::version() const {
    ASSERT_forbid(versions_.isEmpty());
    return versions_.greatest();
}

void
GhostPackage::version(const VersionNumber &v) {
    versions_.insert(v);
}

VersionNumbers
GhostPackage::versions() const {
    return versions_;
}

bool
GhostPackage::isValidVersion(const VersionNumber &v) const {
    return versions().exists(v);
}

std::vector<PackagePattern>
GhostPackage::dependencyPatterns() const {
    ASSERT_not_null(defn_);
    std::vector<PackagePattern> retval;
    if (isParasite()) {
        VersionNumber vPrefix = versionPrefix();
        retval.push_back(PackagePattern(defn_->name() + "-" + vPrefix.toString()));
    } else {
        retval = defn_->dependencyPatterns(version());
    }
    return retval;
}

VersionNumber
GhostPackage::versionPrefix() const {
    std::vector<std::string> parts;
    VersionNumbers vns = versions();
    BOOST_FOREACH (const VersionNumber &vn, vns.values()) {
        if (vn.parts().size() > parts.size())
            parts.resize(vn.parts().size());
        for (size_t i=0; i<vn.parts().size(); ++i) {
            if (parts[i].empty()) {
                parts[i] = vn.parts()[i];
            } else if (parts[i] != "*" && parts[i] != vn.parts()[i]) {
                parts[i] = "*";
            }
        }
    }
    for (size_t i=0; i<parts.size(); ++i) {
        if (parts[i] == "*") {
            parts.resize(i);
            break;
        }
    }
    return boost::join(parts, ".");
}

std::string
GhostPackage::toString() const {
    std::string s = name();

    if (versions().size() > 1) {
        VersionNumber vprefix = versionPrefix();
        if (vprefix.isEmpty()) {
            s += "=*";
        } else {
            s += "=" + vprefix.toString() + ".*";
        }
    } else {
        s += "=" + version().toString();
    }
    
    ASSERT_require(hash().empty());
    if (s.empty())
        s = "empty";
    return s;
}

bool
GhostPackage::isParasite() const {
    return name() != defn_->name();
}

Packages
GhostPackage::parasites() const {
    Packages retval;
    std::vector<Aliases> aliases;
    std::vector<PackagePattern> patterns = defn_->parasitePatterns(version(), aliases /*out*/);
    ASSERT_require(patterns.size() == aliases.size());

    for (size_t i=0; i<patterns.size(); ++i) {
        const PackagePattern &pattern = patterns[i];

        VersionNumbers parasiteVersions;
        if (pattern.version().isEmpty()) {
            parasiteVersions = versions();
        } else {
            parasiteVersions.insert(pattern.version());
        }
        retval.push_back(GhostPackage::instance(defn_, parasiteVersions));
        retval.back()->name(pattern.name());            // this is what makes it a parasite
        retval.back()->aliases(aliases[i]);
    }
    return retval;
}

Package::Ptr
GhostPackage::install(Context &ctx, const VersionNumber &version, Packages &parasites /*out*/) {
    ASSERT_forbid(version.isEmpty());
    parasites.clear();
    Package::Ptr retval;

    if (isParasite()) {
        // Parasites depend on their host, and therefore the host is already installed and has already installed the
        // parasite.  The problem is we no longer have record to tie this parasite ghost package to a particular installed
        // parasite.
        ASSERT_not_implemented("[Robb P Matzke 2017-02-01]");
    } else {
        DefinedPackage::Settings settings;
        settings.version = version;
        settings.keepTempFiles = globalKeepTempFiles;
        retval = definition()->install(ctx, settings);
        parasites = settings.parasites;
    }
    return retval;
}

} // namespace
