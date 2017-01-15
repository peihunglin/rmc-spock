#ifndef Spock_TemporaryDirectory_H
#define Spock_TemporaryDirectory_H

#include <Spock/Exception.h>

#include <boost/filesystem.hpp>

namespace Spock {

/** Create a temporary directory and remove it when done. */
class TemporaryDirectory {
    boost::filesystem::path path_;
    bool keep_;

public:
    TemporaryDirectory(const boost::filesystem::path &path)
        : keep_(false) {
        if (!boost::filesystem::create_directories(path))
            throw Exception::CommandError("cannot create directory: " + path.string()); // exists already, otherwise exception
        path_ = path;                                                                   // now that we created it
    }

    ~TemporaryDirectory() {
        if (!keep_)
            boost::filesystem::remove_all(path_);
    }

    const boost::filesystem::path path() const {
        return path_;
    }

    // Prevent deletion
    void keep() { keep_ = true; }
};

} // namespace

#endif
