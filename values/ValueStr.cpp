#include <cstring>
#include <cstdlib>

#include "ValueStr.hpp"

void DowowNetwork::ValueStr::DeleteBuffer() {
    if (str_data) {
        delete[] str_data;
        str_data = 0;
        str_length = 0;
    }
}

DowowNetwork::ValueStr::ValueStr(std::string str) : Value(ValueTypeStr) {
    Set(str);
}

DowowNetwork::ValueStr::ValueStr(const char *str) : ValueStr(std::string(str)) {
    
}

uint32_t DowowNetwork::ValueStr::DeserializeInternal(const char* data, uint32_t length) {
    // delete the buffer
    DeleteBuffer();

    // read the length
    str_length = le32toh(*reinterpret_cast<const uint32_t*>(data));

    // check length
    if (length < sizeof(str_length)) return 0;

    // check if wrong length
    if (length != str_length + 4) {
        DeleteBuffer();
        return 0;
    }

    if (str_length) {
        // create the buffer
        str_data = new char[str_length];
        // copy
        memcpy(str_data, data + 4, str_length);
    }

    return length;
}

const char* DowowNetwork::ValueStr::SerializeInternal() const {
    char* copy = new char[GetSizeInternal()];

    // temp length in LE
    uint32_t temp = htole32(str_length);
    memcpy(copy, reinterpret_cast<void*>(&temp), sizeof(temp));

    // copy the data
    if (str_length)
        memcpy(copy + 4, str_data, str_length);

    return copy;
}

uint32_t DowowNetwork::ValueStr::GetSizeInternal() const {
    // string itself + 4 bytes of its length in the beginning
    return str_length + 4;
}

std::string DowowNetwork::ValueStr::ToStringInternal(uint16_t indent) const {
    if (str_length == 0)
        return "string[0]: <empty>";

    // increase indent by 4
    indent += 4;

    // the indent string
    std::string indent_str = "\n";
    for (int i = 0; i < indent; i++) indent_str += ' ';
    // the string to return
    std::string return_str = std::string(str_data, str_length);

    // replacing newlines
    size_t replace_index = 0;
    while (true) {
        replace_index = return_str.find("\n", replace_index);
        if (replace_index == std::string::npos) break;

        return_str.replace(replace_index, 1, indent_str);
        replace_index += indent_str.size();
    }
    return "string[" + std::to_string(str_length) + "]: \"" + indent_str + return_str + "\"";
}

std::string DowowNetwork::ValueStr::Get() const {
    if (str_length == 0) return "";

    return std::string(str_data, str_length);
}

void DowowNetwork::ValueStr::Set(const std::string& val) {
    DeleteBuffer();

    str_length = val.size();
    str_data = new char[val.size()];

    memcpy(str_data, val.c_str(), str_length);
}

void DowowNetwork::ValueStr::Set(const char* val, uint32_t len) {
    DeleteBuffer();

    str_length = len;
    str_data = new char[len];

    memcpy(str_data, val, str_length);
}

void DowowNetwork::ValueStr::CopyFrom(Value* original) {
    Set(static_cast<ValueStr*>(original)->Get());
}

DowowNetwork::ValueStr::operator std::string() const {
    return Get();
}

DowowNetwork::ValueStr::~ValueStr() {
    DeleteBuffer();
}
