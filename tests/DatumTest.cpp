#include "../Datum.hpp"
#include "../values/All.hpp"

#include <string>
#include <iostream>
#include <fstream>

using namespace std;
using namespace DowowNetwork;

template<class V, class I> void TestInteger(string temp_path, I num) {
    V val = num;
    cout << "*** Testing number '" << num << "', ValueType==" << (int)val.GetType() << endl;
    cout << "Value size: " << val.GetSize() << endl;
    cout << "Get() result: " << val.Get() << endl;
    val.Set(num + 1);
    cout << "Set(num + 1), so Get() returns: " << val.Get() << endl;
    cout << "Enveloping the Value into Datum named '" << num << "'..." << endl;
    Datum dat;
    dat.SetName(std::to_string(num));
    dat.SetValue(&val);
    cout << "Datum name: " << dat.GetName() << endl;
    cout << "Datum value pointer address: " << dat.GetValue() << " (valid if == " << &val << ")" << endl;
    cout << "Serializing to " << temp_path << "..." << endl;
    char* data = dat.Serialize();
    ofstream fd_w(temp_path);
    if (fd_w.good()) {
        fd_w.write(data, dat.GetSize());
    } else {
        cout << "Couldn't open file!" << endl;
        free(data);
        return;
    }
    fd_w.close();
    delete[] data;

    cout << "Serialized, trying to deserialize..." << endl;
    char* buf = new char[1024];
    ifstream fd_r(temp_path);
    if (fd_r.good()) {
        fd_r.read(buf, 1024);
    }  else {
        cout << "Couldn't read file!" << endl;
        delete[] buf;
        return;
    }
    fd_r.close();
    Datum* d_read = new Datum();
    uint32_t d_res = d_read->Deserialize(buf, 1024);
    cout << "Bytes used to deserialize: " << d_res << "(valid if ==" << dat.GetSize() << ")" << endl;
    delete[] buf;

    cout << "DName: " << d_read->GetName() << endl;
    Value* d_val = d_read->GetValue();
    cout << "DValue type: " << (int)d_val->GetType() << " (valid if ==" << (int)val.GetType() << ")" << endl;
    if (d_val->GetType() != val.GetType()) {
           cout << "Types mismatch!" << endl;
           delete d_read;
           return;
    }
    V* d_val_t = dynamic_cast<V*>(d_val);
    cout << "DValue Get(): " << d_val_t->Get() << endl;

    delete d_read;
}

int main(int argc, char** argv) {
    // the path where temp data will be saved
    string temp_path = std::string(argv[0]) + ".hex";
    
    TestInteger<Value64S, int64_t>(temp_path, 64646464);
    TestInteger<Value64U, uint64_t>(temp_path, 646464641);
    TestInteger<Value32S, int32_t>(temp_path, 32323232);
    TestInteger<Value32U, uint32_t>(temp_path, 323232321);
    TestInteger<Value16S, int16_t>(temp_path, 1616);
    TestInteger<Value16U, uint16_t>(temp_path, 16161);
    TestInteger<Value8S, int8_t>(temp_path, 68);
    TestInteger<Value8U, uint8_t>(temp_path, 69);

    return 0;
}
