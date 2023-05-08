#include <cstring>
#include <iostream>
#include <cstdlib>

#include "Value64S.hpp"

DowowNetwork::Value64S::Value64S(int64_t value) : Value(ValueType64S) {
    this->value = value;
}

uint32_t DowowNetwork::Value64S::DeserializeInternal(const char* data, uint32_t length) {
    // check if length is valid
    if (length < sizeof(value)) return 0;

    // read as uint64_t
    uint64_t temp = *reinterpret_cast<const uint64_t*>(data);
    // LE to host BO
    temp = le64toh(temp);
    // convert to int64_t
    value = *reinterpret_cast<int64_t*>((void*)&temp);

    return sizeof(value);
}

char* DowowNetwork::Value64S::SerializeInternal() {
    char* copy = (char*)malloc(GetSizeInternal());
    // converting the signed to unsigned without changing bits
    uint64_t temp = *reinterpret_cast<uint64_t*>((void*)&value);
    // converting to little endian
    temp = htole64(temp);
    // copy to result
    memcpy(copy, &temp, GetSizeInternal());

    return copy;
}

uint32_t DowowNetwork::Value64S::GetSizeInternal() {
    return sizeof(value);
}

std::string DowowNetwork::Value64S::ToStringInternal(uint16_t indent) {
    return "int64_t: " + std::to_string(value);
}

void DowowNetwork::Value64S::Set(int64_t value) {
    this->value = value;
}

int64_t DowowNetwork::Value64S::Get() const {
    return value;
}

void DowowNetwork::Value64S::CopyFrom(Value* original) {
    this->value = static_cast<Value64S*>(original)->value;
}

DowowNetwork::Value64S::operator int64_t() const {
    return value;
}

DowowNetwork::Value64S::~Value64S() {

}
