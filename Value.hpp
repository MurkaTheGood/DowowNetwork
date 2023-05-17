/*!
    \file

    This file defines the base class for all values.
*/

#ifndef __DOWOW_NETWORK__VALUE_
#define __DOWOW_NETWORK__VALUE_

namespace DowowNetwork {
    class Value;
}

#include <string>
#include <endian.h>

#include "ValueType.hpp"

namespace DowowNetwork {
    /// The base class for all the Value types
    class Value {
    protected:
        /// The value type
        //! \sa ValueType.hpp.
        const uint8_t type;

        /// The function that performs deserialization.
        /*!
            This function must be defined in derived classes those
            describe certain value types.

            \param data pointer to the content of the Value.
            \param length the length of the content.
            \return 
                    - On success: The amount of bytes used
                    to deserialize the value.
                    - On failure: 0.

            \warning    In the body of this function you must never ever
                        delete the data parameter!
            \sa SerializeInternal(), GetSizeInternal().
        */
        virtual uint32_t DeserializeInternal(const char* data, uint32_t length) = 0;
        /// The function that performs serialization.
        /*!
            This function must be defined in derived classes those
            describe certain value types.

            \return 
                    - On success: a pointer to the content buffer, that
                    can be deserialized with DeserializeInternal() function.
                    - On failure: 0.
            \warning
                    The memory inside this function must be allocated using
                    malloc() call! The allocated data must not be stored
                    anywhere, it must be returned and forgotten by you!

            \sa DeserializeInternal(), GetSizeInternal().
        */
        virtual char* SerializeInternal() = 0;
        
        /// The function that calculates the size of the serialized Value.
        /*!
            \return The size of the buffer allocated in function
                    SerializeInternal().
            \sa SerializeInternal().
        */
        virtual uint32_t GetSizeInternal() = 0;
        
        /// The function that converts the Value to string.
        /*!
            \param indent the indentation level (in spaces).
            \return The string representation of the Value.
        */
        virtual std::string ToStringInternal(uint16_t indent);
    public:
        /// Create a value of specified type.
        /*!
            This constructor is to be called in derived classes.
            \param type the type of the Value
            \sa ValueType.hpp.
        */
        Value(uint8_t type);

        /// Deserialize the Value from bytes stream.
        /*!
            \param data the pointer to the buffer that start with the Value metadata.
            \param length the length of the buffer passed as parameter data.
            \return 
                    - On success: the amount of bytes used to deserialize the
                      Value.
                    - On failure: 0.
            \warning
                    The length must be the length of the entire buffer, not
                    the assumed length of the Value.
        */
        uint32_t Deserialize(const char* data, uint32_t length);
        /// Serialize the Value to bytes stream.
        /*!
            \return 
                    - On success: the buffer with the serialized Value.
                    - On failure: 0.
            \warning Use delete[] operator to deallocate the memory!
            \sa GetSize()
        */
        char* Serialize();
        /// Get the size of the buffer returned by Serialize().
        /*! 
            \returns The length of the buffer returned by Serialize().
            \sa Serialize()
        */
        uint32_t GetSize();

        /// Create a deep copy of the original Value.
        /*!
            This function copies the original Value into the Value
            the function is being called on.

            \param original the value that is being copied.
        */
        virtual void CopyFrom(Value* original) = 0;

        /// Get the type of the Value.
        /*!
            \return The type of the Value.
            \sa ValueType.hpp.
        */
        uint8_t GetType();

        /// Convert the Value to string.
        /*!
            To be used for debugging and logging purposes. Calls
            ToStringInternal() function.
            \param indent the indentation level in spaces.
            \return A string that contains the string represenatation
                    of the Value.
        */
        std::string ToString(uint16_t indent = 0);

        /// Value destructor.
        virtual ~Value();
    };

};

#endif
