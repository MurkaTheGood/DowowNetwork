#ifndef __DOWOW_NETWORK__VALUE_32S_
#define __DOWOW_NETWORK__VALUE_32S_

#include "../Value.hpp"

namespace DowowNetwork {
    class Value32S : public Value {
    private:
        int32_t value;
    protected:
        uint32_t DeserializeInternal(const char* data, uint32_t length);
        const char* SerializeInternal() const;
        uint32_t GetSizeInternal() const;
        std::string ToStringInternal(uint16_t indent) const;
    public:
        Value32S(int32_t value = 0);

        // setter and getter
        void Set(int32_t value);
        int32_t Get() const;

        void CopyFrom(Value* original);

        // opeartor to convert to int32_t
        operator int32_t() const;

        ~Value32S();
    };
}

#endif
