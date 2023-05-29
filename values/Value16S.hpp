#ifndef __DOWOW_NETWORK__VALUE_16S_
#define __DOWOW_NETWORK__VALUE_16S_

#include "../Value.hpp"

namespace DowowNetwork {
    class Value16S : public Value {
    private:
        int16_t value;
    protected:
        uint32_t DeserializeInternal(const char* data, uint32_t length);
        const char* SerializeInternal() const;
        uint32_t GetSizeInternal() const;
        std::string ToStringInternal(uint16_t indent) const;
    public:
        Value16S(int16_t value = 0);

        // setter and getter
        void Set(int16_t value);
        int16_t Get() const;

        void CopyFrom(Value* original);

        // opeartor to convert to int16_t
        operator int16_t() const;

        ~Value16S();
    };
}

#endif
