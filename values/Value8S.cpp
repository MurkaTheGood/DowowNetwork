#include <cstring>
#include <iostream>
#include <cstdlib>

#include "Value8S.hpp"

DowowNetwork::Value8S::Value8S(int8_t value) : Value(ValueType8S) {
    this->value = value;
}

uint32_t DowowNetwork::Value8S::DeserializeInternal(const char* data, uint32_t length) {
    // check if length is valid
    if (length < sizeof(value)) return 0;

    value = *reinterpret_cast<const uint8_t*>(data);

    return sizeof(value);
}

char* DowowNetwork::Value8S::SerializeInternal() {
    char* copy = (char*)malloc(GetSizeInternal());
    copy[0] = reinterpret_cast<char*>(&value)[0];
    return copy;
}

uint32_t DowowNetwork::Value8S::GetSizeInternal() {
    return sizeof(value);
}

std::string DowowNetwork::Value8S::ToStringInternal(uint16_t indent) {
    return "int8_t: " + std::to_string((int)value);
}

void DowowNetwork::Value8S::Set(int8_t value) {
    this->value = value;
}

int8_t DowowNetwork::Value8S::Get() const {
    return value;
}

void DowowNetwork::Value8S::CopyFrom(Value* original) {
    this->value = static_cast<Value8S*>(original)->value;
}

DowowNetwork::Value8S::operator int8_t() const {
    return value;
}

DowowNetwork::Value8S::~Value8S() {

}
