/*!
    \file

    Declares some utility functions.
*/

#ifndef __DOWOW_NETWORK__UTILS_H_
#define __DOWOW_NETWORK__UTILS_H_

namespace DowowNetwork {
    namespace Utils {
        /// Check if data can be written to the file descriptor.
        /*!
            Effectively calls select() with zero timeout.

            \param fd file descriptor to check

            \return
                true if data can be written to the file descriptor
                immidiately.
        */
        bool SelectWrite(int fd);

        /// Check if data can be read from the file descriptor.
        /*!
            Effectively calls select() with zero timeout.

            \param fd file descriptor to check

            \return
                true if data can be read from the file descriptor
                immidiately.
        */
        bool SelectRead(int fd);
    };
};

#endif
