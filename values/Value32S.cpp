#include <cstring>
#include <iostream>
#include <cstdlib>

#include "Value32S.hpp"

DowowNetwork::Value32S::Value32S(int32_t value) : Value(ValueType32S) {
    this->value = value;
}

uint32_t DowowNetwork::Value32S::DeserializeInternal(const char* data, uint32_t length) {
    // check if length is valid
    if (length < sizeof(value)) return 0;

    // read as uint32_t
    uint32_t temp = *reinterpret_cast<const uint32_t*>(data);
    // LE to host BO
    temp = le32toh(temp);
    // convert to int32_t
    value = *reinterpret_cast<int32_t*>((void*)&temp);

    return sizeof(value);
}

const char* DowowNetwork::Value32S::SerializeInternal() const {
    char* copy = new char[GetSizeInternal()];
    // converting the signed to unsigned without changing bits
    uint32_t temp = *reinterpret_cast<uint32_t*>((void*)&value);
    // converting to little endian
    temp = htole32(temp);
    // copy to result
    memcpy(copy, &temp, GetSizeInternal());

    return copy;
}

uint32_t DowowNetwork::Value32S::GetSizeInternal() const {
    return sizeof(value);
}

std::string DowowNetwork::Value32S::ToStringInternal(uint16_t indent) const {
    return "int32_t: " + std::to_string(value);
}

void DowowNetwork::Value32S::Set(int32_t value) {
    this->value = value;
}

int32_t DowowNetwork::Value32S::Get() const {
    return value;
}

void DowowNetwork::Value32S::CopyFrom(Value* original) {
    this->value = static_cast<Value32S*>(original)->value;
}

DowowNetwork::Value32S::operator int32_t() const {
    return value;
}

DowowNetwork::Value32S::~Value32S() {

}
