#ifndef __DOWOW_NETWORK__VALUE_16U_
#define __DOWOW_NETWORK__VALUE_16U_

#include "../Value.hpp"

namespace DowowNetwork {
    class Value16U : public Value {
    private:
        uint16_t value;
    protected:
        uint32_t DeserializeInternal(const char* data, uint32_t length);
        char* SerializeInternal();
        uint32_t GetSizeInternal();
        std::string ToStringInternal(uint16_t indent);
    public:
        Value16U(uint16_t value = 0);

        // setter and getter
        void Set(uint16_t value);
        uint16_t Get() const;

        void CopyFrom(Value* original);

        // opeartor to convert to uint16_t
        operator uint16_t() const;

        ~Value16U();
    };
}

#endif
