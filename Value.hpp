#ifndef __DOWOW_NETWORK__VALUE_
#define __DOWOW_NETWORK__VALUE_

namespace DowowNetwork {
    class Value;
}

#include <string>
#include <endian.h>

#include "ValueType.hpp"

namespace DowowNetwork {
    class Value {
    protected:
        const uint8_t type;

        // !Define this function in derived classes!
        // !Do not delete data*!
        // data* points to the content.
        // length is the length of content, as specified in header.
        // This function must return the count of bytes used to deserialize the value from content.
        // If there was a deserialization error, it must return 0.
        virtual uint32_t DeserializeInternal(const char* data, uint32_t length) = 0;
        // !Define this function is derived classes!
        // !Allocate the memory inside the function using malloc()!
        // !Do not store the allocated memory - it will be free()d in outer code!
        // On success: returns a pointer to the content buffer.
        // On failure: returns 0.
        virtual char* SerializeInternal() = 0;
        // get the size of content
        virtual uint32_t GetSizeInternal() = 0;
        // convert the value to string for logging and debugging
        virtual std::string ToStringInternal(uint16_t indent);
    public:
        // Initialize a value of specified type
        Value(uint8_t type);

        // Deserialize the Value.
        // data* is a pointer to the header followed by content.
        // length is the length of data* buffer.
        // Returns amount of bytes used to deserialize the Value.
        // Return 0 if failed to deserialize.
        uint32_t Deserialize(const char* data, uint32_t length);
        // serialize the value for storage and transfer (the memory is allocated for result, and you have to delete it yourself)
        // The memory is allocated using 'new char[...]'
        // Delete the memory using 'delete[]' operator!
        char* Serialize();
        // get the length of the buffer returned by Serialize()
        uint32_t GetSize();

        // create the deepcopy of other value
        virtual void CopyFrom(Value* original) = 0;

        // Get the type
        uint8_t GetType();

        // Convert the Value to string for logging and debugging purpose
        std::string ToString(uint16_t indent = 0);

        virtual ~Value();
    };

};

#endif
