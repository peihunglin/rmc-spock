static const char *purpose = "cause an installed package to be available for use";
static const char *description =
    "Given an installation hash, run a command in a subshell where that installed package and its runtime dependencies have "
    "been added to the environment.  If no command is specified then run an interactive subshell.  If the shell exits with "
    "with zero status then this command also exits with zero status. If the shell command was executed but failed, then this "
    "command exits with status 2. All other failures exit with status 1.";

#include <Spock/Context.h>
#include <Spock/DefinedPackage.h>
#include <Spock/Exception.h>
#include <Spock/GhostPackage.h>
#include <Spock/Package.h>
#include <Spock/PackagePattern.h>
#include <Spock/Solver.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/regex.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <Sawyer/LineVector.h>

using namespace Spock;
using namespace Sawyer::Message::Common;

namespace {

Sawyer::Message::Facility mlog;

enum AutoAnswer { ASSUME_NO=0, ASK_USER, ASSUME_YES };

struct Settings {
    boost::filesystem::path changeCwd;                      // change to this directory before doing anything
    std::vector<std::string> pkgPatterns;                   // package patterns we should use in the subshell
    std::vector<boost::filesystem::path> pkgSelectionFiles; // file containing more package name patterns
    boost::filesystem::path outputFile;                     // write selected packages to this file
    bool showingWelcomeMessage;                             // show a welcome message if a shell-script is started
    AutoAnswer installMissing;                              // what to do about missing packages
    boost::filesystem::path graphVizDeps;                   // file in which to write dependency graph

    Settings()
        : showingWelcomeMessage(false), installMissing(ASSUME_NO) {}
};

std::vector<std::string>
parseCommandLine(int argc, char *argv[], Settings &settings) {
    using namespace Sawyer::CommandLine;

    Parser p = commandLineParser(purpose, description, mlog);
    p.doc("Synopsis", "@prop{programName} [@v{switches}] [--] [@v{command}...]");

    SwitchGroup tool("Tool-specific switches");

    tool.insert(Switch("", 'C')
                .argument("directory", anyParser(settings.changeCwd))
                .doc("Change to the specified working directory before doing anything."));

    tool.insert(Switch("with", 'w')
                .argument("package", listParser(anyParser(settings.pkgPatterns)))
                .whichValue(SAVE_ALL)
                .explosiveLists(true)
                .doc("This switch lists the packages that will be employed in the subshell, in addition to those packages "
                     "that are already employed in the shell where this command is run."));

    tool.insert(Switch("with-file")
                .argument("filename", listParser(anyParser(settings.pkgSelectionFiles), ":"))
                .whichValue(SAVE_ALL)
                .explosiveLists(true)
                .doc("Name of a file that contains packages that should be used.  This switch can appear multiple times "
                     "and/or the argument can be a colon-separated list of file names."));

    tool.insert(Switch("output", 'o')
                .argument("filename", anyParser(settings.outputFile))
                .doc("Name of a file to which package names are written if this command is successful."));

    tool.insert(Switch("graph")
                .argument("filename", anyParser(settings.graphVizDeps))
                .doc("Write solution dependency information to the specified GraphViz file. The result can be viewed many "
                     "ways, one of which is to run \"dot -Tpng -o graph.png @v{filename} && feh graph.png\"."));

    tool.insert(Switch("welcome")
                .intrinsicValue(true, settings.showingWelcomeMessage)
                .doc("Show a welcome message after all checking is completed and just before starting the subshell. The "
                     "purpose is to remind users that they're in a new shell."));

    tool.insert(Switch("install")
                .argument("how", enumParser(settings.installMissing)
                          ->with("no", ASSUME_NO)
                          ->with("yes", ASSUME_YES)
                          ->with("ask", ASK_USER))
                .doc("Try to install missing packages that are missing?  The @v{how} value is one of the following words:"
                     "@named{no}{Do not try to install anything. If something is missing, show and error and exit.}"
                     "@named{ask}{Prompt the user for an answer at each step.}"
                     "@named{yes}{Install missing packages using defaults for each choice. This has the same effect as using "
                     "\"ask\" and pressing enter to use the default value at each prompt, except the output will be less "
                     "verbose.}"));

    ParserResult cmdline = p.with(tool).parse(argc, argv);
    std::vector<std::string> retval = cmdline.unreachedArgs();
    if (retval.empty())
        Sawyer::Message::mfacilities.control("info");
    cmdline.apply();

    BOOST_FOREACH (const std::string &pattern, settings.pkgPatterns) {
        try {
            PackagePattern pp(pattern);
        } catch (const Exception::SyntaxError &e) {
            mlog[FATAL] <<e.what() <<"\n";
            exit(1);
        }
    }

    return retval;
}

bool
yes(const std::string &prompt, const std::string dflt="y") {
    std::cout <<prompt <<" ";
    if (!dflt.empty())
        std::cout <<"[" <<dflt <<"] ";
    std::string answer;
    std::getline(std::cin, answer);
    boost::trim(answer);
    if (answer.empty())
        answer = dflt;
    return !answer.empty() && ('y'==answer[0] || 'Y'==answer[0]);
}

std::string
askString(const std::string &prompt, const std::string &dflt = "") {
    std::cout <<prompt <<" ";
    if (!dflt.empty())
        std::cout <<"[" <<dflt <<"] ";
    std::string answer;
    std::getline(std::cin, answer);
    boost::trim(answer);
    if (answer.empty() || answer=="y" || answer=="Y" || answer=="yes" || answer=="Yes" || answer=="YES")
        return dflt;
    return answer;
}

VersionNumber
askInstall(const Package::Ptr &pkg, AutoAnswer aa) {
    VersionNumbers vns = pkg->versions();
    ASSERT_forbid(vns.isEmpty());

    switch (aa) {
        case ASSUME_NO:
            return VersionNumber();
        case ASSUME_YES:
            return vns.greatest();
        case ASK_USER:
            if (1 == vns.size()) {
                if (yes("install " + pkg->toString() + "?"))
                    return vns.least();
                return VersionNumber();
            }

            BOOST_FOREACH (const VersionNumber &v, vns.values())
                std::cout <<"  " <<v.toString() <<"\n";

            while (true) {
                VersionNumber answer = askString("install " + pkg->name() + "?", vns.greatest().toString());
                if (vns.exists(answer))
                    return answer;
                std::cout <<"invalid response; type a version listed above or press enter to choose the default\n";
            }
    }
    ASSERT_not_reachable("AutoAnswer not handled");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace


int
main(int argc, char *argv[]) {
    Spock::initialize(mlog);
    DefinedPackage::mlog[INFO].enable();
    Context::mlog[MARCH].enable();
    Settings settings;
    std::vector<std::string> command = parseCommandLine(argc, argv, settings);
    boost::regex whitespace("\\s+");

    if (!settings.changeCwd.empty() && chdir(settings.changeCwd.native().c_str()) == -1) {
        mlog[FATAL] <<"cannot change directories to " <<settings.changeCwd <<": " <<strerror(errno) <<"\n";
        exit(1);
    }

    try {
        Spock::Context ctx;

        // Suck in package name patterns from files
        BOOST_FOREACH (const boost::filesystem::path &fileName, settings.pkgSelectionFiles) {
            Sawyer::Container::LineVector lines(fileName);
            for (size_t i=0; lines.lineChars(i); ++i) {
                std::string line = boost::trim_copy(lines.lineString(i));
                std::vector<std::string> words;
                boost::split_regex(words, line, whitespace);
                BOOST_FOREACH (const std::string word, words) {
                    if (boost::starts_with(word, "#"))
                        break;
                    settings.pkgPatterns.push_back(word);
                }
            }
        }

        // Convert pattern strings to patterns
        std::vector<PackagePattern> patterns;
        BOOST_FOREACH (const std::string &patternStr, settings.pkgPatterns) {
            if (!patternStr.empty())
                patterns.push_back(patternStr);
        }

        // Try to find a solution.
        Solver solver(ctx);
        solver.solve(patterns);
        mlog[INFO] <<"solver took " <<solver.nSteps() <<" steps\n";
        if (solver.nSolutions() == 0) {
            solver.showMessages(mlog);
            mlog[ERROR] <<"no solutions found\n";
            exit(1);
        }
        Packages soln = solver.solution(0);
        ctx.sortByDependencyLattice(soln);
        if (!settings.graphVizDeps.empty()) {
            std::ofstream gv(settings.graphVizDeps.string().c_str());
            gv <<ctx.toGraphViz(ctx.dependencyLattice(soln));
        }

        // Are there any missing packages?
        bool partsMissing = false;
        BOOST_FOREACH (const Package::Ptr &pkg, soln) {
            if (!pkg->isInstalled()) {
                partsMissing = true;
                break;
            }
        }

        // Show the entire solution
        BOOST_FOREACH (const Package::Ptr &pkg, soln) {
            if (pkg->isInstalled()) {
                mlog[INFO] <<"  using " <<pkg->toString() <<"\n";
            } else {
                partsMissing = true;
                mlog[ERROR] <<"missing " <<pkg->toString() <<"\n";
                VersionNumbers vns = pkg->versions();
                if (vns.size() > 1) {
                    mlog[INFO] <<"  " <<pkg->name() <<" available versions:";
                    BOOST_FOREACH (const VersionNumber &v, vns.values())
                        mlog[INFO] <<" " <<v.toString();
                    mlog[INFO] <<"\n";
                }
            }
        }
        if (partsMissing && ASSUME_NO == settings.installMissing)
            exit(1);

        // Install missing packages. This loop may output to standard output and read from standard input, but only when
        // running in interactive mode.
        if (partsMissing) {
            Packages inUse;
            for (size_t i=0; i<soln.size(); ++i) {
                if (soln[i]->isInstalled()) {
                    inUse.push_back(soln[i]);
                } else {
                    VersionNumber version = askInstall(soln[i], settings.installMissing);
                    if (version.isEmpty())
                        exit(1);

                    ctx.insertEmployed(inUse);
                    Packages parasites;
                    Package::Ptr newPkg = asGhost(soln[i])->install(ctx, version, parasites /*out*/);
                    inUse.push_back(newPkg);

                    // Any parasites installed just now will replace matching ghosts we encounter in the future
                    BOOST_FOREACH (const Package::Ptr &parasite, parasites) {
                        for (size_t j=i+1; j<soln.size(); ++j) {
                            if (!soln[j]->isInstalled() && soln[j]->name() == parasite->name()) {
                                soln[j] = parasite;
                                break;
                            }
                        }
                    }
                }
            }
            soln = inUse;

            // Overwrite the graphviz file with new info now that we've installed packages
            if (!settings.graphVizDeps.empty()) {
                std::ofstream gv(settings.graphVizDeps.string().c_str());
                gv <<ctx.toGraphViz(ctx.dependencyLattice(soln));
            }
        }
        ctx.insertEmployed(soln);

        // Save output in file?
        if (!settings.outputFile.empty()) {
            std::ofstream file(settings.outputFile.string().c_str());
            BOOST_FOREACH (const Package::Ptr &pkg, soln)
                file <<pkg->toString() <<"\n";
        }

        // Run command in subshell
        if (settings.showingWelcomeMessage) {
            std::cout <<"\n"
                      <<"You are being placed into a new subshell whose environment is configured\n"
                      <<"as you have requested.  You can further customize this environment by\n"
                      <<"running additional spock-shell commands and dropping into deeper recursive\n"
                      <<"subshells. When you're done, you can exit this shell to return to your\n"
                      <<"previous environment. In Bash, the $SHLVL variable will indicate your\n"
                      <<"nesting level.\n\n";
        }

        switch (ctx.subshell(command)) {
            case Context::COMMAND_SUCCESS:
                return 0;
            case Context::COMMAND_FAILED:
                return 1;
            case Context::COMMAND_NOT_RUN:
                return 2;
        }
    
    } catch (const Exception::SpockError &e) {
        mlog[ERROR] <<e.what() <<"\n";
        exit(1);
    }
}
