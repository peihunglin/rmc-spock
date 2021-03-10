static const char *purpose = "run a command and filter results";
static const char *description =
    "This is very similar to creating a pipeline in the shell, but with some key differences to make things work "
    "better when the primary command is a build tool.  This tool has the following features:"

    "@bullet{This tool catches certain signals and then signals the primary command (which is probably a build command). "
    "This works better than signaling the filter because build commands for large projects often have long periods where "
    "they produce no output and would thus not recognize that the output pipes are closed. It also allows the filter to "
    "shut down gracefully when it realizes its input is closed, and therefore show final statistics.}"

    "@bullet{The exit status of this tool is the exit status of the primary (build) command rather than the filter. But "
    "if the primary command succeeds, then the exit status is the filter's exit status.}"

    "@bullet{The filter is optional. If not present then the output from the primary command is shown directly.}";


#include <Spock/Spock.h>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/shared_ptr.hpp>
#include <poll.h>
#include <signal.h>
#include <string>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>

using namespace Spock;
using namespace Sawyer::Message::Common;


static Sawyer::Message::Facility mlog;                  // error and other diagnostic messages
static sig_atomic_t wasInterrupted = 0;                 // set by the signal handler to indicate immediate shutdown
static pid_t primaryPid = 0;                            // PID for the primary command
static pid_t filterPid = 0;                             // PID for the filter. Zero means no filter.
static int primaryOutput[2] = {-1, -1};                 // pipe for the primary command's standard output
static int primaryError[2] = {-1, -1};                  // pipe for the primary command's standard error
static int filterInput[2] = {-1, -1};                   // pipe for the filter command's standard input
static std::vector<std::string> filterCmd;              // command for filtering
static int programExitStatus = 0;                       // final program exit status

// Signal handler: use only signal safe code here
static void signalHandler(int signum) {
    wasInterrupted = signum;
}

// Current time
static time_t
now() {
    return time(NULL);
}

// Do something immediately, and then wait until either a certain time or until some condition is met.
class DoThenWait {
    time_t waitUntil_;                                  // latest time that the next() function is called.
public:
    DoThenWait()
        : waitUntil_(0) {}
    explicit DoThenWait(time_t until)
        : waitUntil_(until) {}
    virtual ~DoThenWait() {}
protected:
    virtual bool waitCondition() = 0;
    virtual void finalAction() = 0;
    void postponeUntil(time_t when) { waitUntil_ = when; }
    bool isCanceled() const { return 0 == waitUntil_; }
    bool isExpired() const { return !isCanceled() && waitUntil_ < now(); }
    void cancel() { waitUntil_ = 0; }
public:
    void process() {
        if (isExpired() || waitCondition()) {
            finalAction();
            cancel();
        }
    }
};

static boost::shared_ptr<DoThenWait> nextEvent;

// Send SIGKILL to filter
class KillFilter: public DoThenWait {
public:
    KillFilter(): DoThenWait(0) {
        if (filterPid > 0) {
            kill(filterPid, SIGKILL);
            SAWYER_MESG(mlog[DEBUG]) <<"sent SIGKILL to filter " <<filterPid <<"\n";
            programExitStatus = 2;
            filterPid = -1;
        }
        exit(programExitStatus);
    }
    bool waitCondition() {
        return true;
    }
    void finalAction() {}
};

// Send SIGTERM to filter
class TermFilter: public DoThenWait {
public:
    TermFilter(): DoThenWait(now() + 5) {
        if (filterPid > 0 && -1 == kill(filterPid, SIGTERM)) {
            filterPid = -1;
            exit(2);
        } else {
            SAWYER_MESG(mlog[DEBUG]) <<"sent SIGTERM to filter " <<filterPid <<"\n";
        }
    }
    bool waitCondition() /*override*/ {
        if (filterPid <= 0)
            return true;
        int status = 0;
        int waited = waitpid(filterPid, &status, WNOHANG);
        if (0 == waited) {
            return false;
        } else if (-1 == waited || WIFSIGNALED(status)) {
            programExitStatus = 2;
            filterPid = -1;
            return true;
        } else if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0)
                programExitStatus = 2;
            filterPid = -1;
            return true;
        } else {
            return false;
        }
    }
    void finalAction() /*override*/ {
        if (filterPid > 0) {
            SAWYER_MESG(mlog[DEBUG]) <<"filter " <<filterPid <<" did not respond to SIGTERM\n";
            nextEvent = boost::make_shared<KillFilter>();
        } else {
            SAWYER_MESG(mlog[DEBUG]) <<"filter terminated\n";
            exit(programExitStatus);
        }
    }
};

// Send SIGINT to filter
class IntFilter: public DoThenWait {
public:
    IntFilter(): DoThenWait(now() + 5) {
        if (filterPid > 0 && -1 == kill(filterPid, SIGINT)) {
            filterPid = -1;
            exit(2);
        } else {
            SAWYER_MESG(mlog[DEBUG]) <<"sent SIGINT to filter " <<filterPid <<"\n";
        }
    }
    bool waitCondition() /*override*/ {
        if (filterPid <= 0)
            return true;
        int status = 0;
        int waited = waitpid(filterPid, &status, WNOHANG);
        if (0 == waited) {
            return false;
        } else if (-1 == waited || WIFSIGNALED(status)) {
            programExitStatus = 2;
            filterPid = -1;
            return true;
        } else if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0)
                programExitStatus = 2;
            filterPid = -1;
            return true;
        } else {
            return false;
        }
    }
    void finalAction() /*override*/ {
        if (filterPid > 0) {
            SAWYER_MESG(mlog[DEBUG]) <<"filter " <<filterPid <<" did not respond to SIGINT\n";
            nextEvent = boost::make_shared<TermFilter>();
        } else {
            SAWYER_MESG(mlog[DEBUG]) <<"filter terminated\n";
            exit(programExitStatus);
        }
    }
};

// Send SIGKILL to primary command
class KillPrimary: public DoThenWait {
public:
    KillPrimary(): DoThenWait(now()) {
        if (primaryPid > 0) {
            kill(primaryPid, SIGINT);
            SAWYER_MESG(mlog[DEBUG]) <<"sent SIGKILL to primary " <<primaryPid <<"\n";
            programExitStatus = 1;
            primaryPid = -1;
        }
    }
    bool waitCondition() /*override*/ {
        return true;
    }
    void finalAction() /*override*/ {
        if (filterPid > 0) {
            nextEvent = boost::make_shared<IntFilter>();
        } else {
            exit(programExitStatus);
        }
    }
};

// Send SIGTERM to primary command
class TermPrimary: public DoThenWait {
public:
    TermPrimary(): DoThenWait(now() + 5) {
        if (primaryPid <= 0 || -1 == kill(primaryPid, SIGTERM)) {
            primaryPid = -1;
            nextEvent = boost::make_shared<IntFilter>();
        } else {
            SAWYER_MESG(mlog[DEBUG]) <<"sent SIGTERM to primary " <<primaryPid <<"\n";
        }
    }
    bool waitCondition() /*override*/ {
        if (primaryPid <= 0)
            return true;
        int status = 0;
        int waited = waitpid(primaryPid, &status, WNOHANG);
        if (0 == waited) {
            return false;
        } else if (-1 == waited || WIFSIGNALED(status)) {
            programExitStatus = 1;
            primaryPid = -1;
            return true;
        } else if (WIFEXITED(status)) {
            programExitStatus = WEXITSTATUS(status);
            primaryPid = -1;
            return true;
        } else {
            return false;
        }
    }
    void finalAction() /*override*/ {
        if (primaryPid > 0) {
            SAWYER_MESG(mlog[DEBUG]) <<"primary " <<primaryPid <<" did not respond to SIGTERM\n";
            nextEvent = boost::make_shared<KillPrimary>();
        } else {
            SAWYER_MESG(mlog[DEBUG]) <<"primary terminated\n";
            nextEvent = boost::make_shared<IntFilter>();
        }
    }
};

// Send SIGINT to the primary command
class IntPrimary: public DoThenWait {
public:
    IntPrimary(): DoThenWait(now() + 5) {
        if (primaryPid <= 0 || -1 == kill(primaryPid, SIGINT)) {
            programExitStatus = 1;
            primaryPid = -1;
            nextEvent = boost::make_shared<IntFilter>();
        } else {
            SAWYER_MESG(mlog[DEBUG]) <<"sent SIGINT to primary " <<primaryPid <<"\n";
        }
    }
    bool waitCondition() /*override*/ {
        if (primaryPid <= 0)
            return true;
        int status = 0;
        int waited = waitpid(primaryPid, &status, WNOHANG);
        if (0 == waited) {
            return false;
        } else if (-1 == waited || WIFSIGNALED(status)) {
            programExitStatus = 1;
            primaryPid = -1;
            return true;
        } else if (WIFEXITED(status)) {
            programExitStatus = WEXITSTATUS(status);
            primaryPid = -1;
            return true;
        } else {
            return false;
        }
    }
    void finalAction() /*override*/ {
        if (primaryPid > 0) {
            SAWYER_MESG(mlog[DEBUG]) <<"primary " <<primaryPid <<" did not respond to SIGINT\n";
            nextEvent = boost::make_shared<TermPrimary>();
        } else {
            SAWYER_MESG(mlog[DEBUG]) <<"primary terminated\n";
            nextEvent = boost::make_shared<IntFilter>();
        }
    }
};

// Wait for primary and filter to exit naturally
class WaitForNaturalExit: public DoThenWait {
public:
    WaitForNaturalExit(): DoThenWait(now() + 60) {}
    bool waitCondition() /*override*/ {
        Stream debug(mlog[DEBUG]);
        SAWYER_MESG(debug) <<"wait for natural exit\n";
        int status = 0;
        if (primaryPid > 0) {
            if (int waited = waitpid(primaryPid, &status, WNOHANG)) {
                ASSERT_require(-1 == waited || waited == primaryPid);
                if (-1 == waited || WIFSIGNALED(status)) {
                    if (debug) {
                        if (-1 == waited) {
                            debug <<"  primary " <<primaryPid <<" wait failed: " <<strerror(errno) <<"\n";
                        } else {
                            debug <<"  primary " <<primaryPid <<" died from signal " <<WTERMSIG(status) <<"\n";
                        }
                    }
                    primaryPid = -1;
                    programExitStatus = 1;
                } else if (WIFEXITED(status)) {
                    SAWYER_MESG(debug) <<"  primary " <<primaryPid <<" exited with status " <<WEXITSTATUS(status) <<"\n";
                    primaryPid = -1;
                    if (WEXITSTATUS(status) != 0)
                        programExitStatus = 1;
                }
            }
        }
        if (filterPid > 0) {
            if (int waited = waitpid(filterPid, &status, WNOHANG)) {
                ASSERT_require(-1 == waited || waited == filterPid);
                if (-1 == waited || WIFSIGNALED(status)) {
                    if (debug) {
                        if (-1 == waited) {
                            debug <<"  filter " <<filterPid <<" wait failed: " <<strerror(errno) <<"\n";
                        } else {
                            debug <<"  filter " <<filterPid <<" died from signal " <<WTERMSIG(status) <<"\n";
                        }
                    }
                    filterPid = -1;
                    programExitStatus = 2;
                } else if (WIFEXITED(status)) {
                    SAWYER_MESG(debug) <<"  filter " <<filterPid <<" exited with status " <<WEXITSTATUS(status) <<"\n";
                    filterPid = -1;
                    if (WEXITSTATUS(status) != 0)
                        programExitStatus = 2;
                }
            }
        }
        return primaryPid <= 0 && filterPid <= 0;
    }

    void finalAction() /*override*/ {
        Stream debug(mlog[DEBUG]);
        if (primaryPid <= 0 && filterPid <= 0) {
            exit(programExitStatus);
        } else {
            SAWYER_MESG(debug) <<"  primary and/or filter did not terminate naturally\n";
            nextEvent = boost::make_shared<IntPrimary>();
        }
    }
};

// Parse command-line switches and return positional arguments
static std::vector<std::string>
parseCommandLine(int argc, char *argv[]) {
    using namespace Sawyer::CommandLine;
    Parser p = commandLineParser(purpose, description, mlog);
    p.doc("Synopsis", "@prop{programName} @v{command} [@v{arguments}...]");

    p.with(Switch("filter", 'F')
           .argument("filter", anyParser(filterCmd))
           .whichValue(SAVE_ALL)
           .doc("Command name of the filter. Filter arguments can be specified with additional "
                "@s{filter} switches, one per argument."));

    boost::filesystem::path cwd = ".";
    p.with(Switch("cwd", 'C')
           .argument("directory", anyParser(cwd))
           .doc("Change to the specified directory before running the primary and filter commands."));

    std::vector<std::string> retval = p.parse(argc, argv).apply().unreachedArgs();
    boost::filesystem::current_path(cwd);
    return retval;
}

// C++ wrapper around exec
static int
execute(const std::vector<std::string> &cmd) {
    char **words = (char**)malloc((cmd.size()+1) * sizeof(char*));
    for (size_t i = 0; i < cmd.size(); ++i)
        words[i] = strdup(cmd[i].c_str());
    words[cmd.size()] = NULL;
    execvp(words[0], words);
    perror("exec failed");
    exit(1);
}

// Start the primary command. Its standard input is closed and its standard output and error are redirected to the two
// supplied pipes.
static pid_t
startPrimary(const std::vector<std::string> &cmd) {
    if (pipe(primaryOutput) == -1 || pipe(primaryError) == -1) {
        perror("pipe failed");
        return 0;
    }

    if (pid_t child = fork()) {
        // This is the parent
        if (-1 == child) {
            perror("fork failed");
            return 0;
        }

        // The child is writing to these, so we don't need to hold them open any longer.
        close(primaryOutput[1]);
        close(primaryError[1]);
        return child;
    } else {
        // This is the child. Close unused files and exec.
        dup2(primaryOutput[1], 1);
        close(primaryOutput[0]);
        close(primaryOutput[1]);
        dup2(primaryError[1], 2);
        close(primaryError[0]);
        close(primaryError[1]);
        execute(cmd);                                   // noreturn
    }
    ASSERT_not_reachable("logic failure build");
}

// Start the filter command
static pid_t
startFilter(const std::vector<std::string> &filterCommand) {
    if (pipe(filterInput) == -1) {
        perror("filter pipe failed");
        return 0;
    }

    if (pid_t child = fork()) {
        // This is the parent
        if (-1 == child) {
            perror("fork failed");
            exit(1);
        }

        // The child is reading from these.
        close(filterInput[0]);
        return child;

    } else {
        // This is the child. Close unused files and exec
        dup2(filterInput[0], 0);
        close(filterInput[0]);
        close(filterInput[1]);
        close(primaryOutput[0]);
        close(primaryError[0]);
        execute(filterCommand);
    }
    ASSERT_not_reachable("logic failure for filter");
}

// Transfers data that might be ready.  If either of the two primary command output streams (stdout or stderr) have data that's
// ready to read, then we read it and forward it to the filter (or our own stdout or stderr if there is no filter).  If the primary
// command closes its output then we close our end also and set it to negative. The stdout takes priority over the stderr.
static void
transferData(pollfd fds[2]) {
    Stream debug(mlog[DEBUG]);
    for (size_t i = 0; i < 2; ++i) {
        if ((fds[i].revents & POLLIN) != 0) {
            uint8_t buf[40960];
            ssize_t nRead = read(fds[i].fd, buf, sizeof buf); // pipes don't block if at least some data is ready
            SAWYER_MESG(debug) <<"  read " <<nRead <<" from primary " <<(i?"stderr":"stdout") <<"\n";
            if (nRead < 0) {
                mlog[FATAL] <<"read from primary command failed: " <<strerror(errno) <<"\n";
                close(fds[i].fd);
                fds[i].fd *= -1;
            } else if (0 == nRead) {            // must be EOF
                SAWYER_MESG(debug) <<"  closed primary " <<(i?"stderr":"stdout") <<"\n";
                close(fds[i].fd);
                fds[i].fd *= -1;
            } else if (filterInput[1] > 0) {     // filter is being used, so write to it
                ssize_t nWrite = write(filterInput[1], buf, nRead);
                ASSERT_always_require2(nWrite == nRead,
                                       (boost::format("nWrite=%d, errno=%s") % nWrite % strerror(errno)).str());
            } else if (i == 0) {
                ssize_t nWrite = write(1, buf, nRead);
                ASSERT_always_require2(nWrite == nRead,
                                       (boost::format("nWrite=%d, errno=%s") % nWrite % strerror(errno)).str());
            } else {
                ssize_t nWrite = write(2, buf, nRead);
                ASSERT_always_require2(nWrite == nRead,
                                       (boost::format("nWrite=%d, errno=%s") % nWrite % strerror(errno)).str());
            }
            break;
        } else if ((fds[i].revents & POLLHUP) != 0) {
            close(fds[i].fd);
            fds[i].fd *= -1;
            SAWYER_MESG(debug) <<"  closed primary " <<(i?"stderr":"stdout") <<"\n";
        }
    }
}

// Transfer data from the two supplied input file descriptors (standard output and error from the primary command)
// to the output file descriptor (the filter's stdin or this tool's stdout) until both inputs are closed.
static void
mainLoop() {
    ASSERT_require(primaryOutput[0] > 0);               // makes it easy to disable in the polling
    ASSERT_require(primaryError[0] > 0);                // makes it easy to disable in the polling
    Stream debug(mlog[DEBUG]);

    // We're going to be polling the primary command's stdout and stderr.
    pollfd fds[2];
    fds[0].fd = primaryOutput[0];
    fds[0].events = POLLIN;
    fds[1].fd = primaryError[0];
    fds[1].events = POLLIN;

    static const unsigned loopInterval = 1000 /*ms*/;

    while (true) {
        if (poll(fds, 2, loopInterval) > 0)
            transferData(fds);

        if (nextEvent) {
            nextEvent->process();
        } else if (wasInterrupted) {
            nextEvent = boost::make_shared<IntPrimary>();
        } else if (fds[0].fd < 0 && fds[1].fd < 0) {
            close(filterInput[1]);
            nextEvent = boost::make_shared<WaitForNaturalExit>();
        }
    }
}

int main(int argc, char *argv[]) {
    Spock::initialize(mlog);
    std::vector<std::string> args = parseCommandLine(argc, argv);

    // setpgrp();

    // Arrange to catch some signals for graceful termination
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction failed");
        exit(1);
    }

    // Start the processes
    primaryPid = startPrimary(args);
    if (!filterCmd.empty())
        filterPid = startFilter(filterCmd);

    // Transfer data from the builder to the filter
    mainLoop();
}
