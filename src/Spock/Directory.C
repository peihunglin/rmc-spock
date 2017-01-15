#include <Spock/Directory.h>

#include <Spock/Package.h>
#include <Spock/PackageLists.h>
#include <Spock/PackagePattern.h>

namespace Spock {

Directory::Directory() {}

Directory::~Directory() {}

// class method
bool
Directory::installedP(const Package::Ptr &pkg) {
    return pkg && pkg->isInstalled();
}

// class method
bool
Directory::notInstalledP(const Package::Ptr &pkg) {
    return pkg && !pkg->isInstalled();
}

// class method
bool
Directory::anyP(const Package::Ptr &pkg) {
    return pkg!=NULL;
}

void
Directory::insert(const Package::Ptr &pkg) {
    ASSERT_not_null(pkg);
    ASSERT_forbid(pkg->name().empty());

    if (!pkg->hash().empty())
        packagesByHash_.insert(pkg->hash(), pkg);
    packagesByName_.insertMaybeDefault(pkg->name()).push_back(pkg);
    BOOST_FOREACH (const std::string &alias, pkg->aliases().values())
        packagesByName_.insertMaybeDefault(alias).push_back(pkg);
}

void
Directory::insert(const Packages &pkgs) {
    BOOST_FOREACH (const Package::Ptr &pkg, pkgs)
        insert(pkg);
}

void
Directory::erase(const Package::Ptr &pkg) {
    ASSERT_not_null(pkg);
    ASSERT_forbid(pkg->name().empty());

    if (!pkg->hash().empty())
        packagesByHash_.erase(pkg->hash());

    if (packagesByName_.exists(pkg->name())) {
        Packages &pkgs = packagesByName_[pkg->name()];
        for (size_t i=0; i<pkgs.size(); ++i) {
            if (pkgs[i]->toString() == pkg->toString())
                pkgs.erase(pkgs.begin()+i);
        }
    }
}

Packages
Directory::find(const PackagePattern &pattern, Predicate constraint) const {
    Packages retval;

    if (!pattern.hash().empty()) {
        if (Package::Ptr pkg = packagesByHash_.getOrDefault(pattern.hash())) {
            ASSERT_require(pkg->isInstalled());
            if (constraint(pkg))
                retval.push_back(pkg);
        }
    } else if (!pattern.name().empty()) {
        BOOST_FOREACH (const Package::Ptr &pkg, packagesByName_.getOrDefault(pattern.name())) {
            if (pattern.matches(pkg) && constraint(pkg))
                retval.push_back(pkg);
        }
    } else {
        BOOST_FOREACH (const Packages &pkgs, packagesByName_.values()) {
            BOOST_FOREACH (const Package::Ptr &pkg, pkgs) {
                if (pattern.matches(pkg) && constraint(pkg))
                    retval.push_back(pkg);
            }
        }
    }

    // Remove duplicate packages, such as when searching for a version number that matches a package that has aliases.
    std::sort(retval.begin(), retval.end());            // sort by pointer value
    retval.erase(std::unique(retval.begin(), retval.end()), retval.end());

    PackageLists::sort(retval);
    return retval;
}

} // namespace
