#include <Spock/Solver.h>

#include <Spock/Exception.h>
#include <Spock/GhostPackage.h>
#include <Spock/InstalledPackage.h>
#include <Spock/Package.h>
#include <Spock/PackageLists.h>

using namespace Sawyer::Message::Common;

namespace Spock {

Sawyer::Message::Facility Solver::mlog;

Solver::Solver(const Context &ctx)
    : ctx_(ctx), maxSolutions_(1), fullSolutions_(true), onlyInstalled_(true), nSteps_(0) {}

Solver::~Solver() {}

const Solver::Solution&
Solver::solution(size_t solutionNumber) const {
    ASSERT_require(solutionNumber < nSolutions());
    return solutions_[solutionNumber];
}

static std::string
indent(size_t level) {
    return std::string(2*level, ' ');
}

void
Solver::insertMessage(const std::string &mesg) {
    messageSet_.insert(mesg);
    latestMessage_ = mesg;
}

size_t
Solver::solve(const PackagePattern &pattern) {
    return solve(std::vector<PackagePattern>(1, pattern));
}

size_t
Solver::solve(const std::vector<PackagePattern> &patterns) {
    mlog[DEBUG] <<"starting solver:\n";
    solutions_.clear();
    messageSet_.clear();
    latestMessage_ = "";
    nSteps_ = 0;

    // Add all employed packages to the constraints, checking that they're all compatible. This should be relatively fast
    // because they should all have hashes and there should be that many of them.
    mlog[DEBUG] <<"  validating packages in use\n";
    Constraints constraints;
    BOOST_FOREACH (const Package::Ptr &pkg, ctx_.employed()) {
        bool needDeps = false;
        constraints = appendConstraint(constraints, pkg, 1, needDeps /*out*/);
        if (constraints.empty())
            return 0;
    }

    PackageLists plists;
    extendLists(constraints, plists /*in,out*/, patterns);
    if (mlog[DEBUG]) {
        if (plists.size() > 0) {
            mlog[DEBUG] <<"  packageLists:\n";
            for (size_t i=0; i<plists.size(); ++i) {
                mlog[DEBUG] <<"    #" <<i <<":\t[";
                for (size_t j=0; j<plists.size(i); ++j)
                    mlog[DEBUG] <<" " <<plists[i][j]->toString();
                mlog[DEBUG] <<" ]\n";
            }
        } else {
            mlog[DEBUG] <<"  packageLists: empty\n";
        }
    }

    if (!plists.isAnyListEmpty()) {
        std::vector<size_t> plistIndexes;
        solve(constraints, plists, plistIndexes);
    }
    return solutions_.size();
}

// For each pattern, find a list matching packages and conditionally append it to plists.  The list is appended only if it
// doesn't already exist in plists.
void
Solver::extendLists(const Constraints &constraints, PackageLists &plists /*in,out*/,
                    const std::vector<PackagePattern> &patterns) {
    BOOST_FOREACH (const PackagePattern &pattern, patterns) {
        if (pattern.name().empty())
            throw Exception::NotFound("no package name in \"" + pattern.toString() + "\"");

        // Get a list of matching packages
        Packages found = ctx_.findPackages(pattern);
        if (!pattern.version().isEmpty()) {
            for (size_t i=0; i<found.size(); ++i) {
                if (!found[i]->isInstalled()) {
                    VersionNumbers matchingVersions;
                    VersionNumbers vns = found[i]->versions();
                    BOOST_FOREACH (const VersionNumber &v, vns.values()) {
                        if (pattern.matches(v))
                            matchingVersions.insert(v);
                    }
                    ASSERT_forbid(matchingVersions.isEmpty());
                    if (matchingVersions != found[i]->versions())
                        found[i] = GhostPackage::instance(asGhost(found[i]), matchingVersions);
                }
            }
        }

        // If we didn't find any packages, then don't even bother continuing. Add the empty list to plists and return. The
        // caller will realize there can't be any solutions.
        if (found.empty()) {
            std::string failure = "no matching packages for " + pattern.toString();
            insertMessage(failure);
            plists.insert(found);
            return;
        }

        // Remove packages that conflict with constraints
        for (size_t i=0; i<found.size(); /*void*/) {
            bool isConflicting = false;
            BOOST_FOREACH (const Package::Ptr &constraint, constraints) {
                if (constraint->excludes(found[i])) {
                    isConflicting = true;
                    std::string failure = found[i]->toString() + " conflicts with " + constraint->toString();
                    insertMessage(failure);
                    break;
                }
            }
            if (isConflicting) {
                found.erase(found.begin()+i);
            } else {
                ++i;
            }
        }
        if (found.empty()) {
            plists.insert(found);                       // to indicate no solution possible
            return;
        }

        // Did we find one match that's already a constraint?
        bool isConstraint = false;
        if (1 == found.size()) {
            BOOST_FOREACH (const Package::Ptr &constraint, constraints) {
                if (found[0]->identical(constraint)) {
                    isConstraint = true;
                    break;
                }
            }
        }

        // Conditionally add the list to the return value.
        if (!isConstraint) {
            plists.sort(found);
            if (!plists.listExists(found))
                plists.insert(found);
        }
    }
}

static bool
sortByString(const Package::Ptr &a, const Package::Ptr &b) {
    return a->toString() < b->toString();
}

static bool
sameString(const Package::Ptr &a, const Package::Ptr &b) {
    return a->toString() == b->toString();
}

// Internal, recursive solver which does a depth-first traversal of the virtual lattice. This solver assumes that the package
// lists indexed by plistIndexes already form a partial solution, and it tries to find all solutions for packages in the next
// list.  For instance, if plistIndexes contains 5 integers, then these 5 integers are indexes into the first five package
// lists in plists and those packages are part of any solutions that are eventually found along this line of reasoning.
void
Solver::solve(const Constraints &constraints, PackageLists &plists, std::vector<size_t> &plistIndexes) {
    ASSERT_require(plistIndexes.size() <= plists.size());
    size_t listNumber = plistIndexes.size();
    size_t callDepth = listNumber + 2;                  // for diagnostics
    ++nSteps_;

    if (mlog[DEBUG]) {
        mlog[DEBUG] <<indent(callDepth-1) <<"solving at level " <<listNumber <<"\n";
        for (size_t i=0; i<plists.size(); ++i) {
            mlog[DEBUG] <<indent(callDepth) <<(i==listNumber?" -> ":"    ") <<"#" <<std::setw(3) <<std::left <<i;
            if (i < listNumber) {
                mlog[DEBUG] <<" " <<plists[i][plistIndexes[i]]->toString();
                if (plists.size(i) > 1)
                    mlog[DEBUG] <<" and " <<(plists.size(i)-1) <<" more";
                mlog[DEBUG] <<"\n";
            } else {
                for (size_t j=0; j<plists.size(i); ++j)
                    mlog[DEBUG] <<" " <<plists[i][j]->toString();
                mlog[DEBUG] <<"\n";
            }
        }
        if (listNumber >= plists.size())
            mlog[DEBUG] <<indent(callDepth) <<" -> end\n";

        mlog[DEBUG] <<indent(callDepth) <<"constraints: ";
        BOOST_FOREACH (const Package::Ptr &constraint, constraints)
            mlog[DEBUG] <<" " <<constraint->toString();
        mlog[DEBUG] <<"\n";
    }
    
    if (solutions_.size() >= maxSolutions_) {
        SAWYER_MESG(mlog[DEBUG]) <<indent(callDepth) <<"enough solutions already\n";
        return;
    }

    // When we reach the end of the lists, we've found a solution.
    if (plistIndexes.size() == plists.size()) {
        Solution soln;
        if (fullSolutions_) {
            soln = constraints;
        } else {
            for (size_t i=0; i<plistIndexes.size(); ++i)
                soln.push_back(plists[i][plistIndexes[i]]);
        }

        // Remove duplicate entries
        std::sort(soln.begin(), soln.end(), sortByString);
        soln.erase(std::unique(soln.begin(), soln.end(), sameString), soln.end());

        // Sort so dependencies come before things that depend on them
        ctx_.sortByDependencyLattice(soln);

        if (mlog[DEBUG]) {
            mlog[DEBUG] <<indent(callDepth) <<"found solution #" <<solutions_.size() <<":";
            BOOST_FOREACH (const Package::Ptr &pkg, soln)
                mlog[DEBUG] <<" " <<pkg->toString();
            mlog[DEBUG] <<"\n";
        }

        solutions_.push_back(soln);
        return;
    }

    // The plistIndexes represent a partial solution -- packages from the first N lists of plists.  Try to extend that solution
    // by adding a package from the next list.
    size_t oldPlistSize = plists.size();
    for (size_t i=0; i<plists.size(listNumber); ++i) {
        ASSERT_require(plistIndexes.size() == listNumber);
        ASSERT_require(plists.size() >= plistIndexes.size());
        plistIndexes.push_back(i);
        Package::Ptr trying = plists[listNumber][i];

        SAWYER_MESG(mlog[DEBUG]) <<indent(callDepth) <<"attempting to extend with #" <<listNumber <<"." <<i <<" "
                                 <<trying->toString() <<"\n";
        bool needDeps = false;
        Constraints newConstraints = appendConstraint(constraints, trying, callDepth+1, needDeps /*out*/);
        if (newConstraints.empty()) {
            SAWYER_MESG(mlog[DEBUG]) <<indent(callDepth) <<"failed to extend with #" <<listNumber <<"." <<i <<" "
                                     <<trying->toString() <<"\n";
        } else {
            if (needDeps) {
                // We added the package itself, now try to add that package's dependencies. We do this indirectly by
                // temporarily adding the dependencies to the lists of packages we're trying to find and then invoking the
                // solver recursively.
                extendLists(newConstraints, plists, trying->dependencyPatterns());
                if (mlog[DEBUG] && plists.size() > oldPlistSize) {
                    mlog[DEBUG] <<indent(callDepth+1) <<"package lists extended with dependencies of "
                                <<trying->toString() <<":";
                    BOOST_FOREACH (const PackagePattern &pp, trying->dependencyPatterns())
                        mlog[DEBUG] <<" " <<pp.toString();
                    mlog[DEBUG] <<"\n";
                    for (size_t j=oldPlistSize; j<plists.size(); ++j) {
                        mlog[DEBUG] <<indent(callDepth+2) <<"#" <<j <<": [";
                        for (size_t k=0; k<plists.size(j); ++k)
                            mlog[DEBUG] <<" " <<plists[j][k]->toString();
                        mlog[DEBUG] <<" ]\n";
                    }
                    if (plists.isAnyListEmpty())
                        mlog[DEBUG] <<indent(callDepth+2) <<"direct conflict with constraints: " <<latestMessage() <<"\n";
                }
            } else {
                SAWYER_MESG(mlog[DEBUG]) <<indent(callDepth+1) <<"package lists need not be extended\n";
            }

            if (!plists.isAnyListEmpty()) {
                solve(newConstraints, plists, plistIndexes);
                if (solutions_.size() >= maxSolutions_) {
                    SAWYER_MESG(mlog[DEBUG]) <<indent(callDepth) <<"enough solutions\n";
                    return;
                }
            }
        }

        // Restore plists and plistIndexes to appropriate sizes for this level by truncating them.
        ASSERT_require(plists.size() >= oldPlistSize);
        plists.resize(oldPlistSize);
        ASSERT_require(plistIndexes.size() >= listNumber);
        plistIndexes.resize(listNumber);
    }
}

// Adds one package (no recursion) to the set of constraints and returns a new set of constraints.  If the package cannot be
// added without violating the existing constraints, then returns the empty constraints.  Upon return, needDeps will be true if
// the constraints changed in such a way that the dependencies of PKG need to be added (false if there's an error or if PKG
// already existed in some form in the constraints).
Solver::Constraints
Solver::appendConstraint(const Constraints &constraints, const Package::Ptr &pkg, size_t callDepth, bool &needDeps /*out*/) {
    ASSERT_not_null(pkg);
    SAWYER_MESG(mlog[DEBUG]) <<indent(callDepth) <<"adding constraint " <<pkg->toString() <<"\n";
    needDeps = false;
    ++callDepth;

    // If the new package conflicts with any package already in the constraints, then the operation fails.
    Constraints retval;
    for (size_t i=0; i<constraints.size(); ++i) {
        const Package::Ptr constraint = constraints[i];

        if (pkg->name() != constraint->name()) {
            // Two different names will not conflict unless they have any of the same aliases, in which case a conflict is
            // guaranteed.  This makes it so that gcc-c++11 cannot be used at the same time as gcc-c++03 since they both have
            // an alias c++-compiler.  In fact, we also detect a conflict if any alias matches a primary name.
            Aliases namesInCommon = pkg->namesInCommon(constraint);
            if (namesInCommon.isEmpty()) {
                retval.push_back(constraint);
            } else {
                std::string failure = pkg->toString() + " and " + constraint->toString() + " have overlapping aliases "
                                      "and therefore cannot be used simultaneously: " + toString(namesInCommon);
                insertMessage(failure);
                SAWYER_MESG(mlog[DEBUG]) <<indent(callDepth) <<failure <<"\n";
                return Constraints();
            }

        } else if (pkg->isInstalled() && constraint->isInstalled()) {
            // If both are installed packages with the same name, then they must have identical hashes. If so, there's nothing
            // more we need to check since the new package exactly matches the constraint.
            if (pkg->hash() == constraint->hash()) {
                SAWYER_MESG(mlog[DEBUG]) <<indent(callDepth) <<"constraint is already present (a)\n";
                retval.insert(retval.end(), constraints.begin()+i, constraints.end());
                return retval;
            } else {
                std::string failure = pkg->toString() + " conflicts with " + constraint->toString();
                insertMessage(failure);
                SAWYER_MESG(mlog[DEBUG]) <<indent(callDepth) <<failure <<"\n";
                return Constraints();
            }

        } else if (!pkg->isInstalled() && !constraint->isInstalled()) {
            // Neither are installed.  There's no solution if their version number sets are disjoint. If they're the same, then
            // there's nothing for us to do. Otherwise, replace the old constraint with the intersection of version numbers and
            // re-validate the subsequent constraints.
            VersionNumbers versions = pkg->versions() & constraint->versions();
            if (versions == constraint->versions()) {
                SAWYER_MESG(mlog[DEBUG]) <<indent(callDepth) <<"constraint is already present (b)\n";
                retval.insert(retval.end(), constraints.begin()+i, constraints.end());
                return retval;
            } else if (!versions.isEmpty()) {
                retval.push_back(GhostPackage::instance(asGhost(constraint), versions));
                for (size_t j=i+1; j<constraints.size(); ++j) {
                    bool dummy = false;
                    retval = appendConstraint(retval, constraints[j], callDepth, dummy /*out*/);
                    if (retval.empty()) {
                        SAWYER_MESG(mlog[DEBUG]) <<indent(callDepth) <<"constraint " <<constraints[j]->toString() <<" violated\n";
                        return Constraints();           // error already reported by recursion
                    }
                }
                return retval;
            } else {
                ASSERT_require(versions.isEmpty());
                std::string failure = "for package " + pkg->name() + ", version sets {";
                BOOST_FOREACH (const VersionNumber &v, pkg->versions().values())
                    failure += " " + v.toString();
                failure += " } and {";
                BOOST_FOREACH (const VersionNumber &v, constraint->versions().values())
                    failure += " " + v.toString();
                failure += " } are disjoint";
                insertMessage(failure);
                SAWYER_MESG(mlog[DEBUG]) <<indent(callDepth) <<failure <<"\n";
                return Constraints();
            }

        } else {
            // One is installed and the other isn't. Check that the installed version matches one of the ghost versions, and if
            // so, keep the installed version since it's a tighter constraint.  Once we tighten that constraint, we need to
            // make sure all subsequent constraints are still valid, which we can do by appending them again one at a time.
            ASSERT_require(pkg->isInstalled() != constraint->isInstalled());
            InstalledPackage::Ptr installed = pkg->isInstalled() ? asInstalled(pkg) : asInstalled(constraint);
            GhostPackage::Ptr ghost = pkg->isInstalled() ? asGhost(constraint) : asGhost(pkg);
            if (ghost->isValidVersion(installed->version())) {
                retval.push_back(installed);
                for (size_t j=i+1; j<constraints.size(); ++j) {
                    bool dummy = false;
                    retval = appendConstraint(retval, constraints[j], callDepth, dummy /*out*/);
                    if (retval.empty()) {
                        SAWYER_MESG(mlog[DEBUG]) <<indent(callDepth) <<"constraint " <<constraints[j]->toString() <<" violated\n";
                        return Constraints();           // error is already reported by recursion
                    }
                }
                if (!constraint->isInstalled()) {
                    // The constraint is a ghost and we're adding an installed package. Since this can tighten the solution, we
                    // need to add the installed package's dependencies also.
                    needDeps = true;
                }
                return retval;
            } else {
                std::string failure = installed->toString() + " does does not match any of {";
                VersionNumbers vns = ghost->versions();
                BOOST_FOREACH (const VersionNumber &v, vns.values())
                    failure += " " + v.toString();
                failure += " }";
                insertMessage(failure);
                SAWYER_MESG(mlog[DEBUG]) <<indent(callDepth) <<failure <<"\n";
                return Constraints();
            }
        }
    }

    // No action taken above, so append this constraint
    needDeps = true;
    retval.push_back(pkg);
    return retval;
}

void
Solver::showMessages(Sawyer::Message::Facility &facility) const {
    if (solutions_.empty()) {
        showMessages(facility[ERROR]);
    } else {
        showMessages(facility[WARN]);
    }
}

void
Solver::showMessages(Sawyer::Message::Stream &stream) const {
    if (stream) {
        BOOST_FOREACH (const std::string &mesg, messageSet_.values())
            stream <<mesg <<"\n";
    }
}

} // namespace
