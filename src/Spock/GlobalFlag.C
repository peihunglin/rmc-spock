#include <Spock/GlobalFlag.h>

namespace Spock {

GlobalFlag::GlobalFlag() {}

GlobalFlag::~GlobalFlag() {}

GlobalFlag::Ptr
GlobalFlag::instance() {
    return Ptr(new GlobalFlag);
}

} // namespace
