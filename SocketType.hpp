#ifndef __DOWOW_NETWORK__SOCKET_TYPE_H_
#define __DOWOW_NETWORK__SOCKET_TYPE_H_

#include <cstdint>

namespace DowowNetwork {
    enum SocketType : uint8_t {
        SocketTypeUndefined = 0,
        SocketTypeUNIX = 1,
        SocketTypeTCP = 2
    };
}

#endif
