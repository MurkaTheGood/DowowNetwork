/*!
    \file

    This file defines the Request.
*/

#ifndef __DOWOW_NETWORK__REQUEST_H_
#define __DOWOW_NETWORK__REQUEST_H_

#include <cstdint>
#include <list>

#include "Datum.hpp"

namespace DowowNetwork {
    /// A basic unit of Dowow Protocol data transfer.
    class Request {
    private:
        /// The list of the arguments.
        std::list<Datum*> arguments;

        /// The ID of the request.
        uint32_t id;

        /// The name of the request.
        std::string name;
    public:
        /// Create a new Request with specified name.
        /*!
            \param name the name of the Request
        */
        explicit Request(std::string name = "");

        /// Set the name of the Request.
        /*!
            \param name the name to be set
        */
        void SetName(std::string name);
        /// Set the ID of the Request.
        /*!
            \param id the ID to be set
        */
        void SetId(uint32_t id);

        /// Get the name of the Request.
        /*!
            \return The name assigned to the Request.
        */
        std::string GetName() const;
        /// Get the ID of the Request.
        /*!
            \return The ID assigned to the Request.
        */
        uint32_t GetId() const;

        /// Set the Request argument.
        /*!
            This method is also used to delete the argument. Deletion
            is performed by passing 0 as value.

            \param name the name of the argument
            \param value the pointer to the value that is to be set
            \param to_copy whether the value must be copied or not

            \warning
                    If the value if not copied then it will be 'stolen'!
                    The 'stolen' value must not be used after calling this
                    method! You must not delete it!

            \sa Set(std::string, Value&).
        */
        void Set(std::string name, Value *value, bool to_copy = true);
        
        /// Set the Request argument.
        /*!
            This method is used to set the argument without pointers usage.
            Effectively calls the Set(std::string, Value*, bool).
            The value is copied.

            \param name the name of the argument
            \param value the value that is to be set
        */
        void Set(std::string name, Value &value);
        /// Get the Request argument.
        /*!
            This method is used to get an argument of the Request.

            \return
                - If the argument is set: pointer to that argument
                - If the argument isn't set: null pointer.

            \warning
                The returned pointer points to the original Value!
                It must never be deleted!
        */
        Value* Get(std::string name);

        /// Emplace the argument.
        /*!
            This method is used to set an argument without
            intermediate code lines. Thanks to this method the assignment
            may be done in one line.

            \param name the name of the argument to emplace
            \param value the value to emplace

            \warning
                Always specify the value type explicitly or you'll
                likely get a compilation error!
        */
        template<class T> void Emplace(std::string name, T value) {
            Set(name, &value, true);
        }

        /// Get the Request argument automatically casted to T.
        /*!
            Effectively calls Get() and dynamic_cast<T>.

            \param name the name of the argument to Get

            \return
                - If types match: T pointer.
                - If types mismatch: null pointer.

            \warning
                The returned pointer points to the original Value!
                It must never be deleted!
        */
        template<class T> T* Get(std::string name) {
            Value *res = Get(name);
            if (!res) return 0;
            return dynamic_cast<T*>(res);
        }

        /// Get the list of arguments.
        /*!
            \return
                The reference to the list of all the arguments.

            \warning
                Never ever delete the elements of that list
                or the list itself! The originals are returned!
        */
        const std::list<Datum*>& GetArguments() const;

        /// Serialize the Request to byte stream.
        /*!
            Creates a buffer containing Request metadata and content.
            The size of that buffer equals to the value returned by GetSize().

            \return
                The buffer containing serialized Request.

            \warning
                Use delete[] operator to delete the returned buffer!
            \sa Deserialize(), GetSize().
        */
        const char* Serialize() const;
        /// Deserialize the Request from byte stream.
        /*!
            \param data pointer to the byte stream beginning
                   with request metadata
            \param data_size the length of the data buffer (not the assumed
                   request length).

            \return
                - On success: the amount of bytes used to deserialize the
                                Request.
                - On failure: 0.

            \warning
                data_size must be the length of the data buffer, not the
                assumed length of the Request!
            \sa Serialize(), GetSize().
        */
        uint32_t Deserialize(char* data, uint32_t data_size);
        /// Get the size of the serialized Request.
        /*!
            \return
                The length of the buffer returned by Serialize().
            \sa Serialize(), Deserialize()
        */
        uint32_t GetSize() const;

        /// Create a deep copy of the Request.
        /*!
            Copies the original Request to the Request that this
            method is being called on.

            \param original the request to copy.
        */
        void CopyFrom(const Request* original);

        /// Convert the Request to a string.
        /*!
            Used for debugging and logging purposes.

            \return
                A string representation of the Request.
        */
        std::string ToString() const;

        /// Request destructor.
        ~Request();
    };
}

#endif
