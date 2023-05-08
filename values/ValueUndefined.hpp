#ifndef __DOWOW_NETWORK__VALUE_UNDEFINED_
#define __DOWOW_NETWORK__VALUE_UNDEFINED_

#include "../Value.hpp"

namespace DowowNetwork {
    class ValueUndefined : public Value {
    private:
        void* undefined_data = 0;
        uint32_t undefined_data_length = 0;
    protected:
        uint32_t DeserializeInternal(const char* data, uint32_t length);
        char* SerializeInternal();
        uint32_t GetSizeInternal();
        std::string ToStringInternal(uint16_t indent);
    public:
        ValueUndefined();

        void CopyFrom(Value* original);

        ~ValueUndefined();
    };
}

#endif
