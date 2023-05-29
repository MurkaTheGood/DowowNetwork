#include <cstring>

#include "Datum.hpp"
#include "values/CreateValue.hpp"

DowowNetwork::Datum::Datum() {
    // undefined value is set
    this->value = new ValueUndefined();
}

bool DowowNetwork::Datum::GetValid() const {
    // the datum is valid if:
    //    1. its value isn't 0
    //    2. its name isn't empty
    return !name.empty() && value;
}

std::string DowowNetwork::Datum::GetName() const {
    return name;
}

uint8_t DowowNetwork::Datum::GetType() const {
    return value->GetType();
}

void DowowNetwork::Datum::SetName(std::string name) {
    this->name = name;
}

DowowNetwork::Value* DowowNetwork::Datum::GetValue() {
    return value;
}

void DowowNetwork::Datum::SetValue(Value* value, bool to_copy) {
    // delete the old value
    delete this->value;

    // set the new value
    if (value) {
        if (to_copy) {
            // copy
            this->value = CreateValue(value->GetType());
            this->value->CopyFrom(value);
        } else {
            // move
            this->value = value;
        }
    }
    else {
        this->value = new ValueUndefined();
    }
}

uint32_t DowowNetwork::Datum::Deserialize(const char* data, uint32_t length) {
    // check if no buffer
    if (!data) return 0;
    // check if length is too small
    if (length < 6) return 0;

    // get the length
    uint32_t total_length = le32toh(*reinterpret_cast<const uint32_t*>(data));

    // check if length is invalid
    if (length < total_length) return 0;

    // get the name length
    uint16_t name_length = le16toh(*reinterpret_cast<const uint16_t*>(data + 4));

    // check if name length is greater than total_length
    if (name_length > total_length) return 0;

    // create a Value of valid type with correct Deserialize() override
    Value* new_value = CreateValue(data[6 + name_length]);

    // deserialize
    uint32_t res = new_value->Deserialize(
        data + 6 + name_length, // offset of value
        total_length - 6 - name_length // length of value
    );

    // failed to deserialize
    if (res == 0) {
        delete new_value;
        return 0;
    }
    // good new value
    delete value;
    value = new_value;

    // name
    this->name = std::string(data + 6, name_length);

    return total_length;
}

char* DowowNetwork::Datum::Serialize() {
    // the total size
    uint32_t size_le = htole32(GetSize());
    // the size of the name
    uint16_t name_size_le = htole16(name.size());

    // allocating the memory
    char* res = new char[GetSize()];

    // fill metadata
    memcpy(res, reinterpret_cast<void*>(&size_le), 4);
    memcpy(res + 4, reinterpret_cast<void*>(&name_size_le), 2);

    // datum name
    memcpy(res + 6, name.c_str(), name.size());

    // fill the value
    const char* value = this->value->Serialize();
    memcpy(res + 6 + name.size(), value, this->value->GetSize());
    delete[] value;

    return res;
}

uint32_t DowowNetwork::Datum::GetSize() {
    return 6 + name.size() + value->GetSize();
}

DowowNetwork::Datum::~Datum() {
    delete value;
}
