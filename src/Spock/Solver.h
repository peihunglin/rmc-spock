#ifndef Spock_Solver_H
#define Spock_Solver_H

#include <Spock/Context.h>

namespace Spock {

/** Solves package constraints. */
class Solver {
public:
    typedef Packages Solution;
    typedef Packages Constraints;

private:
    const Context &ctx_;
    size_t maxSolutions_;                               // max number of solutions to find
    std::vector<Solution> solutions_;
    Sawyer::Container::Set<std::string> messageSet_;
    std::string latestMessage_;
    bool fullSolutions_;                                // if true, then include all dependencies in solutions
    bool onlyInstalled_;                                // find solutions that have only installed packages
    size_t nSteps_;                                     // number of steps performed to find solution(s)

public:
    static Sawyer::Message::Facility mlog;

    Solver(const Context &ctx);
    ~Solver();

    /** Maximum number of solutions to find before stopping.
     *
     * @{ */
    size_t maxSolutions() const { return maxSolutions_; }
    void maxSolutions(size_t n) { maxSolutions_ = n; }
    /** @} */

    /** Whether to report all dependencies are only top-level.
     *
     *  This does not affect the solving, only the reported results.
     *
     * @{ */
    bool fullSolutions() const { return fullSolutions_; }
    void fullSolutions(bool b) { fullSolutions_ = b; }
    /** @} */

    /** Whether to find solutions that have only installed packages.
     *
     * @{ */
    bool onlyInstalled() const { return onlyInstalled_; }
    void onlyInstalled(bool b) { onlyInstalled_ = b; }
    /** @} */

    /** Find solutions.
     *
     *  Given a single pattern or a list of patterns for packages that are required (combined with the list of packages that
     *  are already being used), find solutions that satisify all the patterns. Returns the number of solutions found.
     *
     * @{ */
    size_t solve(const PackagePattern&);
    size_t solve(const std::vector<PackagePattern>&);
    /** @} */

    /** Number of solutions found. */
    size_t nSolutions() const { return solutions_.size(); }

    /** Return a previously found solution. */
    const Solution& solution(size_t solutionNumber) const;

    /** Error messages from last solve attempt.
     *
     *  Note that even when a solution is found there may be error messages that result during the search process, and that if
     *  no solution is found there might be several reasons why. */
    const Sawyer::Container::Set<std::string>& messages() const { return messageSet_; }

    /** Convenient way to show error or warning messages.
     *
     *  Shows all the messages. If at least one solution was found then the messages are warnings, otherwise they're errors.
     *  If you need more control over that, pass a stream instead of a facility.
     *
     * @{ */
    void showMessages(Sawyer::Message::Facility&) const;
    void showMessages(Sawyer::Message::Stream&) const;
    /** @} */

    /** Number of steps performed to find previous solutions. */
    size_t nSteps() const { return nSteps_; }


private:
    // These internal functions are documented in the .C file.
    void insertMessage(const std::string&);
    const std::string& latestMessage() const { return latestMessage_; }
    void extendLists(const Constraints&, PackageLists &plists /*in,out*/, const std::vector<PackagePattern>&);
    void solve(const Constraints&, PackageLists&, std::vector<size_t> &plistIndexes);
    Constraints appendConstraint(const Constraints&, const PackagePtr&, size_t callDepth, bool &needDeps /*out*/);
};

} // namespace

#endif
