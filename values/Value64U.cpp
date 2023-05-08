#include <cstring>
#include <iostream>
#include <cstdlib>

#include "Value64U.hpp"

DowowNetwork::Value64U::Value64U(uint64_t value) : Value(ValueType64U) {
    this->value = value;
}

uint32_t DowowNetwork::Value64U::DeserializeInternal(const char* data, uint32_t length) {
    // check if length is valid
    if (length < sizeof(value)) return 0;

    // assign
    value = le64toh(*reinterpret_cast<const uint64_t*>(data));

    return sizeof(value);
}

char* DowowNetwork::Value64U::SerializeInternal() {
    char* copy = (char*)malloc(GetSizeInternal());

    // converting to little endian
    uint64_t temp = htole64(value);
    // copy to result
    memcpy(copy, &temp, GetSizeInternal());

    return copy;
}

uint32_t DowowNetwork::Value64U::GetSizeInternal() {
    return sizeof(value);
}

std::string DowowNetwork::Value64U::ToStringInternal(uint16_t indent) {
    return "uint64_t: " + std::to_string(value);
}

void DowowNetwork::Value64U::Set(uint64_t value) {
    this->value = value;
}

uint64_t DowowNetwork::Value64U::Get() const {
    return value;
}

void DowowNetwork::Value64U::CopyFrom(Value* original) {
    this->value = static_cast<Value64U*>(original)->value;
}

DowowNetwork::Value64U::operator uint64_t() const {
    return value;
}

DowowNetwork::Value64U::~Value64U() {

}
