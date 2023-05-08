#ifndef __DOWOW_NETWORK__VALUE_STR_
#define __DOWOW_NETWORK__VALUE_STR_

#include <string>

#include "../Value.hpp"

namespace DowowNetwork {
    class ValueStr : public Value {
    private:
        // with terminating null
        char* str_data = 0;
        // without terminating null
        uint32_t str_length = 0;

        void DeleteBuffer();
    protected:
        uint32_t DeserializeInternal(const char* data, uint32_t length);
        char* SerializeInternal();
        uint32_t GetSizeInternal();
        std::string ToStringInternal(uint16_t indent);
    public:
        ValueStr(std::string str = "");
        ValueStr(const char* str);

        // getters and setters
        std::string Get() const;
        void Set(const std::string& val);
        // the provided buffer is copied
        void Set(const char* buffer, uint32_t length);

        void CopyFrom(Value* original);

        // operator
        operator std::string() const;

        ~ValueStr();
    };
}

#endif
