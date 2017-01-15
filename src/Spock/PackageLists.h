#ifndef Spock_PackageLists_H
#define Spock_PackageLists_H

#include <Spock/Spock.h>

namespace Spock {

/** List of lists of packages.
 *
 * This class contains a list of lists of packages. For instance, if the user wants to use boost-1.62 and gcc-4.8, then the
 * list-of-lists might contain two sublists: one that has 10 installations of boost-1.62 with various compilers, and the other
 * that has two installed versions of gcc-4.8 with various patch levels.  Probably not all combinations of an installed boost
 * from the first list and an installed compiler from the second list will be valid. The goal is to choose just one boost
 * that's compatible with one compiler. */
class PackageLists {
    std::vector<Packages> lists_;

public:
    PackageLists();
    ~PackageLists();

    /** Number of sublists. */
    size_t size() const { return lists_.size(); }

    /** Size of a particular sublist. */
    size_t size(size_t listNumber) const;

    /** Make the package lists smaller.
     *
     *  Reduces this package lists to contain only @p n lists. @p n must not be larger than the current number of lists. */
    void resize(size_t n);

    /** A particular package sublist. */
    const Packages& operator[](size_t listNumber) const;

    /** True if this object has no package lists. */
    bool isEmpty() const { return lists_.empty(); }

    /** True if any list exists but is empty. */
    bool isAnyListEmpty() const;

    /** Insert a list of packages. */
    void insert(const Packages&);

    /** Insert a list which is a singleton. */
    void insert(const PackagePtr&);

    /** True if the specified list is already present.
     *
     *  In order to be deemed present, the list must have the same number of packages and they are compared elementwise using
     *  Package::identical. */
    bool listExists(const Packages&) const;

    /** Sort individual package lists.
     *
     *  This sorts packages so the "best" packages are at the front of the list. */
    void sortPackages();

    /** Sort lists by size.
     *
     *  Rearrange the sublists within the top-level list so shorter lists are before longer lists. This makes the search
     *  algorithm more efficient. */
    void sortLists();

    /** Complete sorting.
     *
     *  Sort the packages within each sublist, and sort the sublists within the top-level list. */
    void sort();

    /** Sort a single list.
     *
     *  This is a class method that sorts any list of packages. */
    static void sort(Packages&);
};

} // namespace

#endif
