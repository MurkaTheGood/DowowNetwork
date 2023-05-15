/*!
    \file

    This file defines a enum for Value types.
*/

#ifndef __DOWOW_NETWORK__VALUE_TYPE_
#define __DOWOW_NETWORK__VALUE_TYPE_

#include <cstdint>

namespace DowowNetwork {
    /// The type of the Value.
    enum ValueType : uint8_t {
        ValueTypeUndefined = 0, /*!< undefined value type, aka raw data */
        ValueType64S = 1, /*!< 64-bit signed integer */
        ValueType64U = 2, /*!< 64-bit unsigned integer */
        ValueType32S = 3, /*!< 32-bit signed integer */
        ValueType32U = 4, /*!< 32-bit unsigned integer */
        ValueType16S = 5, /*!< 16-bit signed integer */
        ValueType16U = 6, /*!< 16-bit unsigned integer */
        ValueType8U = 7, /*!< 8-bit unsigned integer */
        ValueType8S = 8, /*!< 8-bit signed integer */
        ValueTypeStr = 9, /*!< string */
        ValueTypeArr = 10 /*!< array */
    };
};

#endif
