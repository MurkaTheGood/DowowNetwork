#include <cstring>
#include <iostream>
#include <cstdlib>

#include "Value16U.hpp"

DowowNetwork::Value16U::Value16U(uint16_t value) : Value(ValueType16U) {
    this->value = value;
}

uint32_t DowowNetwork::Value16U::DeserializeInternal(const char* data, uint32_t length) {
    // check if length is valid
    if (length < sizeof(value)) return 0;

    // assign
    value = le16toh(*reinterpret_cast<const uint16_t*>(data));

    return sizeof(value);
}

char* DowowNetwork::Value16U::SerializeInternal() {
    char* copy = (char*)malloc(GetSizeInternal());

    // converting to little endian
    uint16_t temp = htole16(value);
    // copy to result
    memcpy(copy, &temp, GetSizeInternal());

    return copy;
}

std::string DowowNetwork::Value16U::ToStringInternal(uint16_t indent) {
    return "uint16_t: " + std::to_string(value);
}

uint32_t DowowNetwork::Value16U::GetSizeInternal() {
    return sizeof(value);
}

void DowowNetwork::Value16U::Set(uint16_t value) {
    this->value = value;
}

uint16_t DowowNetwork::Value16U::Get() const {
    return value;
}

void DowowNetwork::Value16U::CopyFrom(Value* original) {
    this->value = static_cast<Value16U*>(original)->value;
}


DowowNetwork::Value16U::operator uint16_t() const {
    return value;
}

DowowNetwork::Value16U::~Value16U() {

}
