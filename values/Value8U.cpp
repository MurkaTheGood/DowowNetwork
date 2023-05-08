#include <cstring>
#include <iostream>
#include <cstdlib>

#include "Value8U.hpp"

DowowNetwork::Value8U::Value8U(uint8_t value) : Value(ValueType8U) {
    this->value = value;
}

uint32_t DowowNetwork::Value8U::DeserializeInternal(const char* data, uint32_t length) {
    // check if length is valid
    if (length < sizeof(value)) return 0;

    // assign
    value = *reinterpret_cast<const uint8_t*>(data);

    return sizeof(value);
}

char* DowowNetwork::Value8U::SerializeInternal() {
    char* copy = (char*)malloc(GetSizeInternal());

    // copy to result
    memcpy(copy, &value, GetSizeInternal());

    return copy;
}

uint32_t DowowNetwork::Value8U::GetSizeInternal() {
    return sizeof(value);
}

std::string DowowNetwork::Value8U::ToStringInternal(uint16_t indent) {
    return "uint8_t: " + std::to_string((int)value);
}

void DowowNetwork::Value8U::Set(uint8_t value) {
    this->value = value;
}

uint8_t DowowNetwork::Value8U::Get() const {
    return value;
}

void DowowNetwork::Value8U::CopyFrom(Value* original) {
    this->value = static_cast<Value8U*>(original)->value;
}

DowowNetwork::Value8U::operator uint8_t() const {
    return value;
}

DowowNetwork::Value8U::~Value8U() {

}
