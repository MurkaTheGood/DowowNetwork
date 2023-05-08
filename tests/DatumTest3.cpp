#include "../Datum.hpp"
#include "../values/All.hpp"

#include <iostream>
#include <fstream>

using namespace std;
using namespace DowowNetwork;

int main(int argc, char** argv) {
    // the path where test will be saved
    string test_path = std::string(argv[0]) + ".hex";

    // create the values
    ValueStr valStr;
    valStr.Set("ABCDEFG");

    cout << "Value: " << valStr.Get() << endl;

    // create a new datum
    Datum *d = new Datum();
    d->SetName("test_value");
    d->SetValue(&valStr);
    char* s = d->Serialize();
    ofstream f(test_path);
    if (f.good()) {
        cout << "Writing..." << endl;
        f.write(s, d->GetSize());
    }
    f.close();
    free(s);

    cout << "Wrote the datum to " << test_path << endl;

    // try to read the datum
    ifstream rd(test_path);
    char* buffer = new char[1024];
    if (rd.good()) {
        rd.read(buffer, 1024);
    }
    rd.close();

    d = new Datum();
    uint32_t res = d->Deserialize(buffer, 1024); 
    cout << "Used " << res << " bytes to decode the data." << endl;
    cout << "Name: " << d->GetName() << endl;
    cout << "Type: " << (int)d->GetType() << endl;
    std::string val_direct = *(ValueStr*)d->GetValue();
    cout << "Value: " << val_direct << endl;

    return 0;
}
