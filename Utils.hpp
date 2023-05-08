#ifndef __DOWOW_NETWORK__UTILS_H_
#define __DOWOW_NETWORK__UTILS_H_

namespace DowowNetwork {
    namespace Utils {
        // returns true if write is possible on the socket
        bool SelectWrite(int fd);
        // returns true if read is possible on the socket
        bool SelectRead(int fd);
    };
};

#endif
