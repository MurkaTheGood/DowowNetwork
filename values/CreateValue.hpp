// this headers implements a function to create the Value object by its type.
#ifndef __DOWOW_NETWORK__CREATE_VALUE_
#define __DOWOW_NETWORK__CREATE_VALUE_

#include "All.hpp"

namespace DowowNetwork {
    // no need for .cpp file, very short function
    static Value* CreateValue(uint8_t type) {
        switch (type) {
            case ValueType64S: return new Value64S();
            case ValueType64U: return new Value64U();
            case ValueType32S: return new Value32S();
            case ValueType32U: return new Value32U();
            case ValueType16S: return new Value16S();
            case ValueType16U: return new Value16U();
            case ValueType8S: return new Value8S();
            case ValueType8U: return new Value8U();
            case ValueTypeStr: return new ValueStr();
            case ValueTypeArr: return new ValueArr();
            
            // values of unknown type are undefined
            default:
                return new ValueUndefined();
        }
    }
}

#endif
