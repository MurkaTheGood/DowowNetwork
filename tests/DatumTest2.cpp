#include "../Datum.hpp"
#include "../values/Value64S.hpp"
#include "../values/Value64U.hpp"
#include "../values/Value32S.hpp"
#include "../values/Value32U.hpp"
#include "../values/Value16S.hpp"
#include "../values/Value16U.hpp"
#include "../values/Value8S.hpp"
#include "../values/Value8U.hpp"


#include <iostream>
#include <fstream>

using namespace std;
using namespace DowowNetwork;

int main(int argc, char** argv) {
    // the path where test will be saved
    string test_path = std::string(argv[0]) + ".hex";

    // create the values
    Value64U val64 = 646464;
    Value32U val32 = 323232;
    Value16U val16 = 1616;
    Value8U val8 = 88;

    cout << "Value sizes:" << 
        "\n\t64U: " << val64.GetSize() <<
        "\n\t32U: " << val32.GetSize() <<
        "\n\t16U: " << val16.GetSize() <<
        "\n\t8U: " << val8.GetSize() <<
        endl;

    // create a new datum
    Datum *d = new Datum();
    d->SetName("test_value");
    d->SetValue(&val8);
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
    uint8_t val_direct = *(Value8U*)d->GetValue();
    cout << "Value: " << val_direct << endl;

    return 0;
}
