#ifndef __DOWOW_NETWORK__SAFE_CONNECTION_H_
#define __DOWOW_NETWORK__SAFE_CONNECTION_H_

#include "Connection.hpp"

namespace DowowNetwork {
    /// Wrapper for connection.
    /// It prevents the connection from being deleted.
    class SafeConnection {
        private:
            Connection *c;
        public:
            SafeConnection() = delete;
            inline SafeConnection(Connection *c) : c(c) {
                c->IncreaseRefs();
            }

            inline Connection& operator()() {
                return *c;
            }

            inline ~SafeConnection() {
                c->DecreaseRefs();
            }
    };
}

#endif
