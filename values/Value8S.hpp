#ifndef __DOWOW_NETWORK__VALUE_8S_
#define __DOWOW_NETWORK__VALUE_8S_

#include "../Value.hpp"

namespace DowowNetwork {
    class Value8S : public Value {
    private:
        int8_t value;
    protected:
        uint32_t DeserializeInternal(const char* data, uint32_t length);
        char* SerializeInternal();
        uint32_t GetSizeInternal();
        std::string ToStringInternal(uint16_t indent);
    public:
        Value8S(int8_t value = 0);

        // setter and getter
        void Set(int8_t value);
        int8_t Get() const;

        void CopyFrom(Value* original);

        // opeartor to convert to int8_t
        operator int8_t() const;

        ~Value8S();
    };
}

#endif
