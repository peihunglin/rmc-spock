#include <Spock/Environment.h>

namespace Spock {

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
Environment::append(const std::string &name, const std::string &value, const std::string &sep) {
    const std::string &oldval = variables_.getOrDefault(name);
    std::string newval = oldval.empty() ? value : oldval + sep + value;
    variables_.insert(name, newval);
}

void
Environment::prepend(const std::string &name, const std::string &value, const std::string &sep) {
    const std::string &oldval = variables_.getOrDefault(name);
    std::string newval = oldval.empty() ? value : value + sep + oldval;
    variables_.insert(name, newval);
}

void
Environment::prepend(const Environment &other) {
    BOOST_FOREACH (const Map::Node &node, other.variables_.nodes())
        prepend(node.key(), node.value());
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
