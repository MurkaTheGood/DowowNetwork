#ifndef __DOWOW_NETWORK__DATUM_
#define __DOWOW_NETWORK__DATUM_

#include "Value.hpp"

#include <string>
#include <cstdint>

namespace DowowNetwork {
    /// A data point in request.
    /*!
        Datum is like a function parameter. It basically serves
        the same purpose.
    */
    class Datum {
    private:
        /// The name of the datum.
        std::string name = "";
        /// The value pointer.
        Value* value;
    public:
        /// Create a new empty datum.
        Datum();

        /// Check if the Datum is a valid one.
        /*!
            For the Datum to be valid, it must:
            1.  have a name that consists only of ASCII letters, digits,
                underscores and hyphens,
            2.  have a name that has length in range between 1 and 32
                (inclusive),
            3.  have a value that is not a null pointer.
            \return true if valid, false if not.
        */
        bool GetValid() const;
        /// Get the name of the Datum.
        /*!
            \return The name of the Datum.
        */
        std::string GetName() const;
        /// Get the type of the Value.
        /*!
            \return The type of the value.
            \sa ValueType.hpp.
        */
        uint8_t GetType() const;

        /// Set the name of the value.
        /*!
            \param name the name to be set.
            \sa GetValid().
        */
        void SetName(std::string name);
        /// Get the pointer to the value of the Datum.
        /*!
            \return The pointer to the value assigned to the value.
        */
        Value* GetValue();
        /// Set a new value for the Datum.
        /*!
            \param value    the pointer to the value to be set.
            \param to_copy  whether the value must be copied or
                            the original must be saved.
            \warning        If to_copy is false then the value will be
                            'stolen' by the Datum and you must not use
                            that value anymore!
        */
        void SetValue(Value* value, bool to_copy = true);

        /// Deserialize the Datum from bytes stream.
        /*!
            \param data the pointer to the bytes array.
            \param length the size of data array.
            \return
                - On success: The amount of bytes used to deserialize the data.
                - On failure: 0.
            \warning    One must always pass the array length as length
                        parameter, not the assumed length of the Datum!
            \sa Serialize().
        */
        uint32_t Deserialize(const char* data, uint32_t length);
        /// Serialize the Datum to bytes stream.
        /*!
            The returned buffer must be deleted using delete[]
            operator. The length of the buffer equals to
            the value returned by GetSize().

            \return
                - On success: The buffer that contains the serialized Datum,
                - On failure: 0.
            \sa Deserialize(), GetSize().
        */
        char* Serialize();
        /// Get the length of serialized Datum.
        /*!
            \return The length of the buffer returned by Serialize().
        */
        uint32_t GetSize();

        /// Datum destructor.
        ~Datum();
    };
};

#endif
