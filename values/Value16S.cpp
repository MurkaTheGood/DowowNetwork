#include <cstring>
#include <iostream>
#include <cstdlib>

#include "Value16S.hpp"

DowowNetwork::Value16S::Value16S(int16_t value) : Value(ValueType16S) {
    this->value = value;
}

uint32_t DowowNetwork::Value16S::DeserializeInternal(const char* data, uint32_t length) {
    // check if length is valid
    if (length < sizeof(value)) return 0;

    // read as uint16_t
    uint16_t temp = *reinterpret_cast<const uint16_t*>(data);
    // LE to host BO
    temp = le16toh(temp);
    // convert to int16_t
    value = *reinterpret_cast<int16_t*>((void*)&temp);

    return sizeof(value);
}

const char* DowowNetwork::Value16S::SerializeInternal() const {
    char* copy = new char[GetSizeInternal()];
    // converting the signed to unsigned without changing bits
    uint16_t temp = *reinterpret_cast<uint16_t*>((void*)&value);
    // converting to little endian
    temp = htole16(temp);
    // copy to result
    memcpy(copy, &temp, GetSizeInternal());

    return copy;
}

uint32_t DowowNetwork::Value16S::GetSizeInternal() const {
    return sizeof(value);
}

std::string DowowNetwork::Value16S::ToStringInternal(uint16_t indent) const {
    return "int16_t: " + std::to_string(value);
}

void DowowNetwork::Value16S::Set(int16_t value) {
    this->value = value;
}

int16_t DowowNetwork::Value16S::Get() const {
    return value;
}

void DowowNetwork::Value16S::CopyFrom(Value* original) {
    this->value = static_cast<Value16S*>(original)->value;
}

DowowNetwork::Value16S::operator int16_t() const {
    return value;
}

DowowNetwork::Value16S::~Value16S() {

}
