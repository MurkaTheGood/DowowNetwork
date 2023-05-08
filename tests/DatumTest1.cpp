#include "../Datum.hpp"
#include <iostream>
#include <fstream>

using namespace std;
using namespace DowowNetwork;

int main(int argc, char** argv) {
    // the path where test will be saved
    string test_path = std::string(argv[0]) + ".bin";

    // create a new datum
    Datum *d = new Datum();
    d->SetName("test");
    d->SetValue(0);

    char* s = d->Serialize();
    /*fstream f(test_path);
    if (f.good()) {
        f.write(s, d->GetSize());
    }
    f.close();*/

    cout << "Wrote the datum to " << test_path << endl;

    // try to read the datum
    ifstream rd(test_path);
    char* buffer = new char[1024];
    if (rd.good()) {
        rd.read(buffer, 1024);
    }
    rd.close();

    d = new Datum();
    cout << "Used " << d->Deserialize(buffer, 1024) << " bytes to decode the data." << endl;
    cout << "name: " << d->GetName() << endl;

    return 0;
}
