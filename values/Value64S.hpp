#ifndef __DOWOW_NETWORK__VALUE_64S_
#define __DOWOW_NETWORK__VALUE_64S_

#include "../Value.hpp"

namespace DowowNetwork {
    class Value64S : public Value {
    private:
        int64_t value;
    protected:
        uint32_t DeserializeInternal(const char* data, uint32_t length);
        const char* SerializeInternal() const;
        uint32_t GetSizeInternal() const;
        std::string ToStringInternal(uint16_t indent) const;
    public:
        Value64S(int64_t value = 0);

        // setter and getter
        void Set(int64_t value);
        int64_t Get() const;

        void CopyFrom(Value* original);

        // opeartor to convert to int64_t
        operator int64_t() const;

        ~Value64S();
    };
}

#endif
