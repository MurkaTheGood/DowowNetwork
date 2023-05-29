#ifndef __DOWOW_NETWORK__VALUE_64U_
#define __DOWOW_NETWORK__VALUE_64U_

#include "../Value.hpp"

namespace DowowNetwork {
    class Value64U : public Value {
    private:
        uint64_t value;
    protected:
        uint32_t DeserializeInternal(const char* data, uint32_t length);
        const char* SerializeInternal() const;
        uint32_t GetSizeInternal() const;
        std::string ToStringInternal(uint16_t indent) const;
    public:
        Value64U(uint64_t value = 0);

        // setter and getter
        void Set(uint64_t value);
        uint64_t Get() const;

        void CopyFrom(Value* original);

        // opeartor to convert to uint64_t
        operator uint64_t() const;

        ~Value64U();
    };
}

#endif
