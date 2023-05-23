/*!
    \file

    Declares some utility functions.
*/

#ifndef __DOWOW_NETWORK__UTILS_H_
#define __DOWOW_NETWORK__UTILS_H_

#include <cstdint>
#include <time.h>

namespace DowowNetwork {
    namespace Utils {
        /// Check if data can be written to the file descriptor.
        /*!
            Effectively calls select() with specified timeout.

            \param fd file descriptor to check
            \param seconds the timeout in seconds

            \return
                true if data can be written to the file descriptor.
                false on timeout.
        */
        bool SelectWrite(int fd, time_t seconds = 0);

        /// Check if data can be read from the file descriptor.
        /*!
            Effectively calls select() with specified timeout.

            \param fd file descriptor to check
            \param seconds the timeout in seconds

            \return
                true if data can be read from the file descriptor.
                false on timeout.
        */
        bool SelectRead(int fd, time_t seconds = 0);

        /// Write uint64_t to event file descriptor.
        /*!
            \param fd the file descriptor to write to
            \param num the number to writer to eventfd
        */
        void WriteToEventFd(int fd, uint64_t num);

        /// Set the timer expiration time.
        void SetTimerFdTimeout(int fd, time_t seconds);
    };
};

#endif
