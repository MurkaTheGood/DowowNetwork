#ifndef __DOWOW_NETWORK__VALUE_8U_
#define __DOWOW_NETWORK__VALUE_8U_

#include "../Value.hpp"

namespace DowowNetwork {
    class Value8U : public Value {
    private:
        uint8_t value;
    protected:
        uint32_t DeserializeInternal(const char* data, uint32_t length);
        const char* SerializeInternal() const;
        uint32_t GetSizeInternal() const;
        std::string ToStringInternal(uint16_t indent) const;
    public:
        Value8U(uint8_t value = 0);

        // setter and getter
        void Set(uint8_t value);
        uint8_t Get() const;

        void CopyFrom(Value* original);

        // opeartor to convert to uint8_t
        operator uint8_t() const;

        ~Value8U();
    };
}

#endif
