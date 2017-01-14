#ifndef Spock_GlobalFlag_H
#define Spock_GlobalFlag_H

#include <Spock/Spock.h>

namespace Spock {

class GlobalFlag: public Sawyer::SharedObject {
public:
    typedef Sawyer::SharedPointer<GlobalFlag> Ptr;

protected:
    GlobalFlag();

public:
    ~GlobalFlag();

    static Ptr instance();
};

} // namespace

#endif
