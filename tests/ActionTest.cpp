#include "../Datum.hpp"
#include "../Action.hpp"
#include "../values/All.hpp"

#include <string>
#include <iostream>
#include <fstream>

using namespace std;
using namespace DowowNetwork;

int main(int argc, char** argv) {
    Action action("get_test_action");

    // Add some numbers as arguments
    Value64S val1 = 1025;
    ValueStr val2 = std::string("Test string value here!");
    ValueArr val3;
    for (int i = 0; i < 30; i++) {
        Value32U a = i;
        val3.Push(&a);
    }
    // gruesome nested array
    ValueArr val4;
    val4.Push(&val3);
    val4.Push(&val4);
    ((ValueArr*)val4.Get(0))->Push(&val4);
    for (int i = 0; i < 15; i++)
        val4.Push(&val4);


    // Add the args
    action.SetArgument("money", &val1);
    action.SetArgument("description", &val2);
    action.SetArgument("senders", &val3);
    action.SetArgument("nested", &val4);

    cout << "Action size: " << action.GetSize() << endl;

    // Save to file
    char* ser = action.Serialize();

    ofstream writer("ActionTest.hex");
    writer.write(ser, action.GetSize());
    writer.close();

    // Now deserialize
    Action *d_action = new Action("haha");
    uint32_t used = d_action->Deserialize(ser, action.GetSize());
    delete[] ser;

    cout << "Used " << used << " bytes to deserialize (must be " << action.GetSize() << ")" << endl;
    cout << "Name: '" << d_action->name << "' (must be '" << action.name << "')" << endl;
    cout << "ARGUMENTS:" << endl;
    Value* d_val1 = d_action->GetArgument("money");
    if (d_val1) {
        cout << "money = " << ((Value64S*)d_val1)->Get() << endl;
    } else {
        cout << "money NOT FOUND!" << endl;
    }

    Value* d_val2 = d_action->GetArgument("description");
    if (d_val1) {
        cout << "description = " << ((ValueStr*)d_val2)->Get() << endl;
    } else {
        cout << "description NOT FOUND!" << endl;
    }

    Value* d_val3 = d_action->GetArgument("senders");
    if (d_val3) {
        ValueArr *arr = (ValueArr*)d_val3;
        cout << "senders.GetCount() = " << arr->GetCount() << endl;
        for (int i = 0; i < arr->GetCount(); i++) {
            cout << "senders[" << i << "] = " << ((Value32U*)arr->Get(i))->Get() << endl;
        }
    } else {
        cout << "senders NOT FOUND!" << endl;
    }

    delete d_action;

    return 0;
}
