#include <cstdlib>
#include <cstring>

#include "Value.hpp"

std::string DowowNetwork::Value::ToStringInternal(uint16_t indent) {
    return
        "type: " + std::to_string((int)type) +
        "; size: " + std::to_string(GetSize()) + "B";
}

DowowNetwork::Value::Value(uint8_t type) : type(type) {
    
}

uint32_t DowowNetwork::Value::Deserialize(const char* data, uint32_t length) {
    // check for basic errors
    if (!data) return 0;
    if (length < 5) return 0;
    if (data[0] != GetType()) return 0;
    
    // trying to get length
    uint32_t len = 
        le32toh(*reinterpret_cast<const uint32_t*>(data + 1));
    
    // checking length
    if (len + 5 > length) return 0;

    // calling the derived deserializer
    return DeserializeInternal(data + 5, len) + 5;
}

char* DowowNetwork::Value::Serialize() {
    // 5 bytes of metadata
    char *res = new char[5 + GetSizeInternal()];

    // set the metadata
    res[0] = GetType();
    // get the length of data
    uint32_t data_length = GetSizeInternal();
    // set the length
    memcpy(res + 1, reinterpret_cast<char*>(&data_length), 4);
    
    // get the data
    char* data = SerializeInternal();
    memcpy(res + 5, data, GetSizeInternal());
    free(data);

    return res;
}

uint32_t DowowNetwork::Value::GetSize() {
    return GetSizeInternal() + 5;
}

uint8_t DowowNetwork::Value::GetType() {
    return type;
}

std::string DowowNetwork::Value::ToString(uint16_t indent) {
    // creating indent
    std::string indent_str = "";
    for (int i = 0; i < indent; i++) indent_str += ' ';
    return indent_str + ToStringInternal(indent);
}

DowowNetwork::Value::~Value() {
    
}
