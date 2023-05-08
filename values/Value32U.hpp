#ifndef __DOWOW_NETWORK__VALUE_32U_
#define __DOWOW_NETWORK__VALUE_32U_

#include "../Value.hpp"

namespace DowowNetwork {
    class Value32U : public Value {
    private:
        uint32_t value;
    protected:
        uint32_t DeserializeInternal(const char* data, uint32_t length);
        char* SerializeInternal();
        uint32_t GetSizeInternal();
        std::string ToStringInternal(uint16_t indent);
    public:
        Value32U(uint32_t value = 0);

        // setter and getter
        void Set(uint32_t value);
        uint32_t Get() const;

        void CopyFrom(Value* original);

        // opeartor to convert to uint32_t
        operator uint32_t() const;

        ~Value32U();
    };
}

#endif
