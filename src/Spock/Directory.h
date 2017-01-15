#ifndef Spock_Directory_H
#define Spock_Directory_H

#include <Spock/Spock.h>

namespace Spock {

/** Directory of all known packages. */
class Directory {
    typedef Sawyer::Container::Map<std::string /*hash*/, PackagePtr> PackagesByHash;
    typedef Sawyer::Container::Map<std::string /*name*/, Packages> PackagesByName;

    PackagesByHash packagesByHash_;                     // all known installed packages indexed by their hash code
    PackagesByName packagesByName_;                     // all known packages (installed or not) indexed by their name

public:
    Directory();
    ~Directory();

    typedef bool(*Predicate)(const PackagePtr&);
    static bool installedP(const PackagePtr&);
    static bool notInstalledP(const PackagePtr&);
    static bool anyP(const PackagePtr&);

    void insert(const PackagePtr&);
    void insert(const Packages&);

    void erase(const PackagePtr&);

    Packages find(const PackagePattern&, Predicate) const;
};

} // namespace

#endif
