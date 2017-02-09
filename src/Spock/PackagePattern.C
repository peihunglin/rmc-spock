#include <Spock/PackagePattern.h>

#include <Spock/Exception.h>
#include <Spock/GhostPackage.h>
#include <Spock/InstalledPackage.h>

#include <boost/regex.hpp>

namespace Spock {

PackagePattern::PackagePattern(const std::string &s) {
    parse(s);
}

void
PackagePattern::parse(const std::string &s) {
    static bool initialized = false;
    static std::vector<std::string> p;
    static Sawyer::Container::Map<std::string, VersOp> ops;

    if (!initialized) {
        // Package name are alphanumeric with some special characters allowed, although "-" can only appear internally.
        std::string pkgName = "(?:[[:alnum:]]+(?:[-+_]+[[:alnum:]]+)*(?:[_+]*))";

        // Part of a version number: word characters but special characters can only be in the interior.
        std::string versionPart = "(?:[[:alnum:]]+(?:[-_]+[[:alnum:]]+)*)";

        // A version number is either two or more parts separated by single "." characters, or a natural number <= 999999
        std::string dottedVersion  = "(?:" + versionPart + "(?:\\." + versionPart + ")+)";
        std::string relaxedVersion = "(?:" + versionPart + "(?:\\." + versionPart + ")*)";
        std::string singleNumber   = "(?:[1-9][0-9]{0,5})";
        std::string dottedOrNumber = "(?:" + dottedVersion + "|" + singleNumber + ")";

        // A comparison operator for version numbers is =, !=, <, <=, >, >= ("-" is intentionally excluded)
        std::string versionOp = "(?:!?=|<=?|>=?)";
        ops.insert("=", VERS_EQ);
        ops.insert("!=", VERS_NE);
        ops.insert("<", VERS_LT);
        ops.insert("<=", VERS_LE);
        ops.insert(">", VERS_GT);
        ops.insert(">=", VERS_GE);
        ops.insert("-", VERS_HY);
        ops.insert("", VERS_EQ);

        // A hash is an "@" followed by eight lower-case hexadecimal characters
        std::string hash = "(?:@[0-9a-f]{8})";

        // Whole patterns
        p.push_back("()()()()");

        // Only a hash: @12345678
        p.push_back("()()()(" + hash + ")");

        // Vers and optional hash: >=1.2, -1.2, -12, -alpha (any of these followed by a hash).  There's no ambiguity here about
        // whether "alpha" in the pattern "-alpha" is a package or version--it's always a version because package names cannot
        // start with a hyphen.
        p.push_back("()(" + versionOp + "|-)(" + relaxedVersion + ")(" + hash + "?)");

        // Pkg followed by non-ambiguous version and optional hash. "yaml-cpp=alpha"
        p.push_back("(" + pkgName + ")(" + versionOp + ")(" + relaxedVersion + ")(" + hash + "?)");

        // Pkg followed by ambiguous version introduced with a hyphen. In this case, the version pattern is slightly tighter so
        // that a string like "foo-alpha" is a package name without a version, but foo-alpha.beta" has a version of
        // "alpha.beta" because of the dot.  Numbers are still versions, as in "foo-1".
        p.push_back("(" + pkgName + ")(-)(" + dottedOrNumber +")(" + hash + "?)");

        // No version
        p.push_back("(" + pkgName + ")()()(" + hash + "?)");

        initialized = true;
    }
    
    bool found = false;
    BOOST_FOREACH (const std::string &re, p) {
        boost::smatch results;
        if (boost::regex_match(s, results, boost::regex(re))) {
#if 0 // DEBUGGING [Robb P Matzke 2017-01-14]
            std::cerr <<"        package = \"" <<results.str(1) <<"\"\n"
                      <<"        versOp  = \"" <<results.str(2) <<"\"\n"
                      <<"        version = \"" <<results.str(3) <<"\"\n"
                      <<"        hash    = \"" <<results.str(4) <<"\"\n";
#endif
            pkgName_ = results.str(1);
            versOp_ = ops[results.str(2)];
            version_ = results.str(3);
            if (!results.str(4).empty())
                hash_ = results.str(4).substr(1);
            found = true;
            break;
        }
    }

    if (!found)
        throw Exception::SyntaxError("invalid package pattern \"" + s + "\"");
}

std::string
PackagePattern::toString() const {
    std::string s = pkgName_;
    if (!version_.isEmpty()) {
        switch (versOp_) {
            case VERS_EQ: s += "="; break;
            case VERS_NE: s += "!="; break;
            case VERS_LT: s += "<"; break;
            case VERS_LE: s += "<="; break;
            case VERS_GT: s += ">"; break;
            case VERS_GE: s += ">="; break;
            case VERS_HY: s += "-"; break;
        }
        s += version_.toString();
    }
    if (!hash_.empty())
        s += "@" + hash_;
    return s;
}

bool
PackagePattern::matches(const VersionNumber &haystack) const {
    if (!version_.isEmpty()) {
        switch (versOp_) {
            case VERS_EQ: return haystack == version_;
            case VERS_NE: return haystack != version_;
            case VERS_LT: return   haystack < version_;
            case VERS_GE: return !(haystack < version_);
            case VERS_LE: return   haystack < version_ || haystack == version_;
            case VERS_GT: return !(haystack < version_ || haystack == version_);
            case VERS_HY: return haystack - version_;
        }
    }

    return true;
}

bool
PackagePattern::matches(const Package::Ptr &pkg) const {
    ASSERT_not_null(pkg);
    if (!pkgName_.empty() && pkgName_ != pkg->name() && !pkg->aliases().exists(pkgName_))
        return false;
    if (!hash_.empty() && hash_ != pkg->hash())
        return false;

    // Installed packages have only one version, but ghost packages have many versions. Return true if this pattern matches any
    // of those versions. */
    if (pkg->isInstalled()) {
        return matches(pkg->version());
    } else {
        VersionNumbers vns = asGhost(pkg)->versions();
        BOOST_FOREACH (const VersionNumber &v, vns.values()) {
            if (matches(v))
                return true;
        }
    }

    return false;
}

} // namespace
