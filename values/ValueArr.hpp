#ifndef __DOWOW_NETWORK__VALUE_ARR_
#define __DOWOW_NETWORK__VALUE_ARR_

#include <vector>
#include <string>

#include "../Value.hpp"

namespace DowowNetwork {
    class ValueArr : public Value {
    private:
        // buffer
        std::vector<Value*> array;
    protected:
        uint32_t DeserializeInternal(const char* data, uint32_t length);
        const char* SerializeInternal() const;
        uint32_t GetSizeInternal() const;
        std::string ToStringInternal(uint16_t indent) const;
    public:
        ValueArr();

        // get element #index.
        // Returns 0 if oob.
        Value* Get(uint32_t index) const;
        // set element #index
        // returns true if successful. False if oob.
        // The array copies the value.
        bool Set(uint32_t index, Value* val);
        // push the element after the last one
        void Push(Value* val);
        // get count of elements
        uint32_t GetCount() const;
        // create a new array of specified size (all elements will be undefined)
        void New(uint32_t count);
        // delete the buffer
        void Clear();

        void CopyFrom(Value* original);

        ~ValueArr();
    };
}

#endif
