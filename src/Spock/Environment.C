#include <Spock/Environment.h>

#include <boost/foreach.hpp>

namespace Spock {

static std::vector<std::string>
split(const std::string &s, const std::string &sep) {
    std::vector<std::string> retval;
    size_t at = 0;
    while (at < s.size()) {
        size_t found = s.find(sep, at);
        if (std::string::npos == found) {
            retval.push_back(s.substr(at));
            break;
        } else {
            retval.push_back(s.substr(at, found - at));
            at = found + sep.size();
        }
    }
    return retval;
}

static std::string
join(const std::vector<std::string> &parts, const std::string &sep) {
    std::string retval;
    for (size_t i = 0; i < parts.size(); ++i)
        retval += (i ? sep : "") + parts[i];
    return retval;
}

void
Environment::reload() {
    variables_.clear();
    for (size_t i=0; environ[i]; ++i) {
        if (const char *eq = strchr(environ[i], '='))
            variables_.insert(std::string(environ[i], eq-environ[i]), eq+1);
    }
}

void
Environment::set(const std::string &name, const std::string &value) {
    variables_.insert(name, value);
}

std::string
Environment::get(const std::string &name, const std::string &dflt) const {
    return variables_.getOrElse(name, dflt);
}

void
Environment::appendUnique(const std::string &name, const std::string &value, const std::string &sep) {
    std::vector<std::string> retvalParts = split(variables_.getOrDefault(name), sep);
    std::vector<std::string> valueParts = split(value, sep);
    BOOST_FOREACH (const std::string &valuePart, valueParts) {
        if (std::find(retvalParts.begin(), retvalParts.end(), valuePart) == retvalParts.end())
            retvalParts.push_back(valuePart);
    }
    variables_.insert(name, join(retvalParts, sep));
}

void
Environment::prependUnique(const std::string &name, const std::string &value, const std::string &sep) {
    std::vector<std::string> retvalParts = split(variables_.getOrDefault(name), sep);
    std::vector<std::string> valueParts = split(value, sep);
    std::reverse(retvalParts.begin(), retvalParts.end());
    std::reverse(valueParts.begin(), valueParts.end());
    BOOST_FOREACH (const std::string &valuePart, valueParts) {
        if (std::find(retvalParts.begin(), retvalParts.end(), valuePart) == retvalParts.end())
            retvalParts.push_back(valuePart);
    }
    std::reverse(retvalParts.begin(), retvalParts.end());
    variables_.insert(name, join(retvalParts, sep));
}

void
Environment::prependUnique(const Environment &other) {
    BOOST_FOREACH (const Map::Node &node, other.variables_.nodes())
        prependUnique(node.key(), node.value());
}

void
Environment::exportVars() const {
    clearenv();
    BOOST_FOREACH (const Map::Node &node, variables_.nodes()) {
        if (!node.value().empty())
            setenv(node.key().c_str(), node.value().c_str(), 1);
    }
}

} // namespace
