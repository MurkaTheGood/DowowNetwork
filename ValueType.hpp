#ifndef __DOWOW_NETWORK__VALUE_TYPE_
#define __DOWOW_NETWORK__VALUE_TYPE_

#include <cstdint>

namespace DowowNetwork {
    enum ValueType : uint8_t {
        ValueTypeUndefined = 0,
        ValueType64S = 1,
        ValueType64U = 2,
        ValueType32S = 3,
        ValueType32U = 4,
        ValueType16S = 5,
        ValueType16U = 6,
        ValueType8U = 7,
        ValueType8S = 8,
        ValueTypeStr = 9,
        ValueTypeArr = 10
    };
};

#endif
