#include <cstring>
#include <cstdlib>

#include "ValueUndefined.hpp"

DowowNetwork::ValueUndefined::ValueUndefined() : Value(ValueTypeUndefined) {
    
}

uint32_t DowowNetwork::ValueUndefined::DeserializeInternal(const char* data, uint32_t length) {
    // no data at all
    if (length == 0) return 0;

    // data of unknown type
    undefined_data = malloc(length);
    undefined_data_length = length;

    memcpy(undefined_data, data, length);

    return length;
}

char* DowowNetwork::ValueUndefined::SerializeInternal() {
    char* copy = (char*)malloc(GetSizeInternal());
    memcpy(copy, undefined_data, GetSizeInternal());
    return copy;
}

uint32_t DowowNetwork::ValueUndefined::GetSizeInternal() {
    return undefined_data_length;
}

std::string DowowNetwork::ValueUndefined::ToStringInternal(uint16_t indent) {
    return "undefined[" + std::to_string(indent) + "]";
}

void DowowNetwork::ValueUndefined::CopyFrom(Value* original) {
    if (undefined_data) free(undefined_data);
    undefined_data =
        malloc(static_cast<ValueUndefined*>(original)->undefined_data_length);
    memcpy(undefined_data, static_cast<ValueUndefined*>(original)->undefined_data, undefined_data_length);
}

DowowNetwork::ValueUndefined::~ValueUndefined() {
    free(undefined_data);
}
