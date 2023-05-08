#ifndef __DOWOW_NETWORK__DATUM_
#define __DOWOW_NETWORK__DATUM_

#include "Value.hpp"

#include <string>
#include <cstdint>

namespace DowowNetwork {
    class Datum {
    private:
        // the name of the datum
        std::string name = "";
        // value
        Value* value;
    public:
        // create an empty datum
        Datum();

        // returns true if Datum is valid, i.e. can be used in Action and Reaction
        bool GetValid() const;
        // returns the name of the Datum
        std::string GetName() const;
        // returns the type of the Datum's Value
        uint8_t GetType() const;

        // set the name
        void SetName(std::string name);
        // returns the Value pointer
        Value* GetValue();
        // sets a new Value.
        void SetValue(Value* value, bool to_copy = true);

        // load the datum from bytes. Returns size of datum on success.
        uint32_t Deserialize(const char* data, uint32_t length);
        // convert the datum to raw bytes to transfer it over network (or store it as file - nevermind).
        // On success: returns a pointer to buffer and writes its length to the reference in argument.
        // On failure: returns 0, the length reference isn't accessed.
        char* Serialize();
        // returns the size of the serialized buffer
        uint32_t GetSize();

        // destroy the datum
        ~Datum();
    };
};

#endif
