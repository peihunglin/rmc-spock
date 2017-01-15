#include <Spock/PackageLists.h>

#include <Spock/InstalledPackage.h>

namespace Spock {

PackageLists::PackageLists() {}

PackageLists::~PackageLists() {}

void
PackageLists::insert(const Packages &pkgs) {
    lists_.push_back(pkgs);
}

void
PackageLists::insert(const Package::Ptr &pkg) {
    lists_.push_back(std::vector<Package::Ptr>(1, pkg));
}

size_t
PackageLists::size(size_t listNumber) const {
    ASSERT_require(listNumber < lists_.size());
    return lists_[listNumber].size();
}

const Packages&
PackageLists::operator[](size_t listNumber) const {
    ASSERT_require(listNumber < lists_.size());
    return lists_[listNumber];
}

bool
PackageLists::isAnyListEmpty() const {
    BOOST_FOREACH (const Packages &list, lists_) {
        if (list.empty())
            return true;
    }
    return false;
}

// Sort alphabetically by package name, and then by descending version number, then by installed before not-installed, then
// descending timestamp.
bool
sortByNameVersion(const Package::Ptr &a, const Package::Ptr &b) {
    if (a->isInstalled() != b->isInstalled())
        return a->isInstalled();                        // installed before not-installed
    if (a->name() != b->name())
        return a->name() < b->name();                   // alphabetical name
    if (a->version() != b->version())
        return b->version() < a->version();             // descending version numbers
    if (a->isInstalled()) {
        InstalledPackage::Ptr ai = asInstalled(a);
        InstalledPackage::Ptr bi = asInstalled(b);
        if (ai->timestamp() != bi->timestamp())
            return bi->timestamp() < ai->timestamp();   // descending timestamps for installed packages
    }
    return a->hash() < b->hash();                       // ascending hash if present
}

// Sort by ascending container size
bool
sortBySize(const Packages &a, const Packages &b) {
    return a.size() < b.size();
}

void
PackageLists::sortPackages() {
    BOOST_FOREACH (Packages &list, lists_)
        std::sort(list.begin(), list.end(), sortByNameVersion);
}

void
PackageLists::sortLists() {
    std::sort(lists_.begin(), lists_.end(), sortBySize);
}

void
PackageLists::sort() {
    sortPackages();
    sortLists();
}

void
PackageLists::sort(Packages &pkgs) {
    std::sort(pkgs.begin(), pkgs.end(), sortByNameVersion);
}

bool
PackageLists::listExists(const Packages &a) const {
    BOOST_FOREACH (const Packages &b, lists_) {
        if (a.size() == b.size()) {
            bool allSame = true;
            for (size_t i=0; allSame && i<a.size(); ++i)
                allSame = a[i]->identical(b[i]);
            if (allSame)
                return true;
        }
    }
    return false;
}

void
PackageLists::resize(size_t n) {
    ASSERT_forbid(n > lists_.size());
    lists_.resize(n);
}

} // namespace
