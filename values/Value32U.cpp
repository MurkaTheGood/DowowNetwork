#include <cstring>
#include <iostream>
#include <cstdlib>

#include "Value32U.hpp"

DowowNetwork::Value32U::Value32U(uint32_t value) : Value(ValueType32U) {
    this->value = value;
}

uint32_t DowowNetwork::Value32U::DeserializeInternal(const char* data, uint32_t length) {
    // check if length is valid
    if (length < sizeof(value)) return 0;

    // assign
    value = le32toh(*reinterpret_cast<const uint32_t*>(data));

    return sizeof(value);
}

char* DowowNetwork::Value32U::SerializeInternal() {
    char* copy = (char*)malloc(GetSizeInternal());

    // converting to little endian
    uint32_t temp = htole32(value);
    // copy to result
    memcpy(copy, &temp, GetSizeInternal());

    return copy;
}

uint32_t DowowNetwork::Value32U::GetSizeInternal() {
    return sizeof(value);
}

std::string DowowNetwork::Value32U::ToStringInternal(uint16_t indent) {
    return "uint32_t: " + std::to_string(value);
}

void DowowNetwork::Value32U::Set(uint32_t value) {
    this->value = value;
}

uint32_t DowowNetwork::Value32U::Get() const {
    return value;
}

void DowowNetwork::Value32U::CopyFrom(Value* original) {
    this->value = static_cast<Value32U*>(original)->value;
}

DowowNetwork::Value32U::operator uint32_t() const {
    return value;
}

DowowNetwork::Value32U::~Value32U() {

}
