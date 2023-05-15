/*!
    \file

    This file declares SocketType enum.
*/

#ifndef __DOWOW_NETWORK__SOCKET_TYPE_H_
#define __DOWOW_NETWORK__SOCKET_TYPE_H_

#include <cstdint>

namespace DowowNetwork {
    /// The type of a socket
    enum SocketType : uint8_t {
        SocketTypeUndefined = 0,    ///< undefined (unset) socket type
        SocketTypeUnix = 1,         ///< UNIX socket
        SocketTypeTcp = 2           ///< TCP socket
    };
}

#endif
