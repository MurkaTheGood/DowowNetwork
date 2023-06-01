#include <algorithm>
#include <cstring>
#include <cstdlib>
#include "Value.hpp"

#include "Request.hpp"

DowowNetwork::Request::Request(std::string name) : name(name) {

}

void DowowNetwork::Request::SetName(std::string name) {
    this->name = name;
}

void DowowNetwork::Request::SetId(uint32_t id) {
    this->id = id;
}

std::string DowowNetwork::Request::GetName() const {
    return name;
}

uint32_t DowowNetwork::Request::GetId() const {
    return id;
}

void DowowNetwork::Request::Set(std::string name, Value* value, bool to_copy) {
    if (Get(name)) {
        // argument is already set
        auto temp_iter = 
            std::find_if(
                arguments.begin(),
                arguments.end(),
                [&name](Datum* dat) { return dat->GetName() == name; }
            );
        (*temp_iter)->SetValue(value, to_copy);
    } else {
        // not set yet
        Datum* new_dat = new Datum();
        new_dat->SetName(name);
        new_dat->SetValue(value, to_copy);
        arguments.push_back(new_dat);
    }
}

void DowowNetwork::Request::Set(std::string name, Value& val) {
    // reuse code
    Set(name, &val, true);
}

DowowNetwork::Value* DowowNetwork::Request::Get(std::string name) {
    auto temp_iter =
        std::find_if(
            arguments.begin(),
            arguments.end(),
            [&name](Datum* dat) { return dat->GetName() == name; }
        );
    if (temp_iter == arguments.end()) return 0;
    return (*temp_iter)->GetValue();
}

const std::list<DowowNetwork::Datum*>& DowowNetwork::Request::GetArguments() const {
    return arguments;
}

const char *DowowNetwork::Request::Serialize() const {
    char *result = new char[GetSize()];

    uint32_t ser_total_length = htole32(GetSize());
    uint32_t ser_id = htole32(id);
    uint16_t ser_name_length = htole16(name.size());

    memcpy(result, &ser_total_length, 4);
    memcpy(result + 4, &ser_id, 4);
    memcpy(result + 8, &ser_name_length, 2);
    memcpy(result + 10, name.c_str(), name.size());

    uint32_t offset = 10 + name.size();
    for (auto& arg : arguments) {
        char* temp_buffer = arg->Serialize();
        memcpy(result + offset, temp_buffer, arg->GetSize());
        delete[] temp_buffer;
        offset += arg->GetSize();
    }

    return result;
}

uint32_t DowowNetwork::Request::Deserialize(char* data, uint32_t data_size) {
    // can't even read the header
    if (data_size < 10) return 0;

    // reading header
    uint32_t des_total_length = *reinterpret_cast<uint32_t*>((void*)data);
    uint32_t des_id = *reinterpret_cast<uint32_t*>((void*)(data + 4));
    uint16_t des_name_length = *reinterpret_cast<uint16_t*>((void*)(data + 8));

    // BO
    des_total_length = le32toh(des_total_length);
    des_id = le32toh(des_id);
    des_name_length = le32toh(des_name_length);

    // check if everything is valid
    if (des_total_length > data_size) return 0;
    if (des_name_length > data_size) return 0;

    // delete old datums
    for (auto i : arguments) delete i;
    arguments.clear();

    // begin deserialization
    id = des_id;
    name = std::string(data + 10, des_name_length);

    // deserialize datums
    uint32_t i = 10 + des_name_length;
    while (i < data_size) {
        Datum* new_datum = new Datum();
        uint32_t temp_res = new_datum->Deserialize(data + i, data_size - i);
        if (!temp_res) {
            delete new_datum;
            break;
        }
        i += temp_res;
        arguments.push_back(new_datum);
    }

    return i;
}

uint32_t DowowNetwork::Request::GetSize() const {
    uint32_t sum = 10 + name.size();
    for (auto& arg : arguments) sum += arg->GetSize();
    return sum;
}

void DowowNetwork::Request::CopyFrom(const Request* original) {
    SetName(original->GetName());
    SetId(original->GetId());
    const auto& original_args = original->GetArguments();
    // copy the arguments
    for (const auto& d: original_args) {
        Set(d->GetName(), d->GetValue());
    }
}

std::string DowowNetwork::Request::ToString() const {
    std::string result = "Request \"" + name + "\":";
    for (auto& datum : arguments) {
        result += "\n* " + datum->GetName() + " -> " + datum->GetValue()->ToString();
    }
    return result;
}

DowowNetwork::Request::~Request() {
    for (auto i : arguments) delete i;
}
