#ifndef __DOWOW_NETWORK__REQUEST_H_
#define __DOWOW_NETWORK__REQUEST_H_

#include <cstdint>
#include <list>

#include "Datum.hpp"

namespace DowowNetwork {
    class Request {
    private:
        std::list<Datum*> arguments;

        // ID is used to indentify the actions within one session.
        // It will be automatically initialized with an incremented number,
        // each time a new Request is created.
        uint32_t id;
        // The name of the action
        std::string name;
    public:
        explicit Request(std::string name = "");

        // Set the name
        void SetName(std::string name);
        // Set the ID
        void SetId(uint32_t id);

        // Get the name
        std::string GetName();
        // Get the ID
        uint32_t GetId();

        // Set the argument by pointer.
        // Set the argument to 0 to unset it.
        void Set(std::string name, Value *value, bool to_copy = true);
        // Set the argument by reference.
        void Set(std::string name, Value &value);
        // Get the argument.
        // Do not delete it, the original is returned.
        // Returns 0 if the argument is not set.
        Value* Get(std::string name);

        // Emplace the argument.
        template<class T> void Emplace(std::string name, T value) {
            Set(name, &value, true);
        }
        // Get the argument automatically converted to specified type.
        // Returns 0 if the argument is not set or type is invalid.
        template<class T> T* Get(std::string name) {
            Value *res = Get(name);
            if (!res) return 0;
            return dynamic_cast<T*>(res);
        }

        // Get the list of arguments.
        std::list<Datum*>& GetArguments();

        // Serialize. See GetSize() to get the size of returned buffer.
        // You must use delete[] to free memory of returned buffer.
        char* Serialize();
        // Deserialize. Returns the amount of bytes used to deserialize the Request.
        // Returns 0 of failed deserialization.
        uint32_t Deserialize(char* data, uint32_t data_size);
        // Get the size of serialized action.
        uint32_t GetSize();

        // Create the deepcopy of other request
        void CopyFrom(Request* original);

        // Convert to string
        std::string ToString();

        ~Request();
    };
}

#endif
