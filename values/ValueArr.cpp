#include <cstring>
#include <cstdlib>
#include "CreateValue.hpp"
#include "ValueArr.hpp"

DowowNetwork::ValueArr::ValueArr() : Value(ValueTypeArr) {
    
}

uint32_t DowowNetwork::ValueArr::DeserializeInternal(const char* data, uint32_t length) {
    // delete the buffer
    Clear();

    // read the elements amound
    uint32_t elements_amount = le32toh(*reinterpret_cast<const uint32_t*>(data));

    // reserve space
    array.reserve(elements_amount);

    uint32_t read_offset = 4;

    // iterate
    for (uint32_t i = 0; i < elements_amount; i++) {
        // out of bounds - invalidate array
        if (read_offset > length) {
            Clear();
            return 0;
        }
        // try to get an element
        uint8_t type = data[read_offset];
        // try to create the value
        Value* val = CreateValue(type);
        // try to deserialize value
        uint32_t read_bytes = val->Deserialize(data + read_offset, length - read_offset);
        // invalid element - invalidate array
        if (read_bytes == 0) {
            delete val;
            Clear();
            return 0;
        }
        // fine
        array.push_back(val);
        // increase offset
        read_offset += read_bytes;
    }
    // return for outer code
    return read_offset;
}

char* DowowNetwork::ValueArr::SerializeInternal() {
    char* copy = (char*)malloc(GetSizeInternal());

    // amount in LE
    uint32_t temp = htole32(array.size());
    memcpy(copy, reinterpret_cast<void*>(&temp), sizeof(temp));

    // offset
    uint32_t write_offset = 4;

    // serialize all elements
    for (auto i : array) {
        char* i_buf = i->Serialize();
        uint32_t i_size = i->GetSize();
        memcpy(copy + write_offset, i_buf, i_size);
        delete[] i_buf;
        write_offset += i_size;
    }

    return copy;
}

uint32_t DowowNetwork::ValueArr::GetSizeInternal() {
    // size of each element + 4 bytes of array length
    uint32_t sum = 4; 
    for (auto i : array) {
        sum += i->GetSize();
    }
    return sum;
}

std::string DowowNetwork::ValueArr::ToStringInternal(uint16_t indent) {
    std::string result = "array of size " + std::to_string(array.size()) + ":";
    std::string indent_str = "";
    for (int i = 0; i < indent + 4; i++) indent_str += ' ';

    uint32_t index = 0;
    for (auto& val : array) {
        result += "\n" + indent_str + "element #" + std::to_string(index++) + ":";
        result += "\n" + val->ToString(indent + 8);
    }
    return result;
}

DowowNetwork::Value* DowowNetwork::ValueArr::Get(uint32_t index) const {
    if (index < 0 || index >= array.size()) return 0;

    return array[index];
}

bool DowowNetwork::ValueArr::Set(uint32_t index, Value* val) { 
    if (index < 0 || index >= array.size()) return false;
    if (!val) return false;

    delete array[index];

    // create a copy
    array[index] = CreateValue(val->GetType());
    array[index]->CopyFrom(val);

    return true;
}

void DowowNetwork::ValueArr::Push(Value* val) {
    // create a copy
    Value* copy = CreateValue(val->GetType());
    copy->CopyFrom(val);

    array.push_back(copy);
}

uint32_t DowowNetwork::ValueArr::GetCount() const {
    return array.size();
}

void DowowNetwork::ValueArr::New(uint32_t count) {
    Clear();
    array.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        array.push_back(new ValueUndefined());
    }
}

void DowowNetwork::ValueArr::Clear() {
    for (size_t i = 0; i < array.size(); i++)
        delete array[i];
    array.clear();
}

void DowowNetwork::ValueArr::CopyFrom(Value* original_) {
    ValueArr* original = static_cast<ValueArr*>(original_);

    New(original->array.size());
    for (size_t i = 0; i < array.size(); i++)
        Set(i, original->Get(i));
}

DowowNetwork::Value*& DowowNetwork::ValueArr::operator[](uint32_t index) {
    return array[index];
}

DowowNetwork::ValueArr::~ValueArr() {
    Clear();
}
