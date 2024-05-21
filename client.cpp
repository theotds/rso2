#include <Ice/Ice.h>
#include <mpk.h>
#include <memory>
#include <vector>
#include <iostream>
#include <limits>

using namespace std;
using namespace MPK;

class UserI : public User {
public:
    void updateStop(TramPrxPtr tram, ::std::shared_ptr<StopPrx> stop, const ::Ice::Current& current) override {
        for (auto& scheduleItem : tram->getSchedule()) {
            cout << "Tram " << tram->getID() << " stops at " << scheduleItem.stop->getName()
                 << " at " << scheduleItem.time.hour << ":" << scheduleItem.time.minute << endl;
        }
        if(!tram->getSchedule().empty()){
            cout<<endl;
        }
    }

    void updateSchedule(StopPrxPtr stop, arrivals arr, const ::Ice::Current& current) override {
        for (auto& arrival : stop->getArrivals()) {
            cout << "At stop " << stop->getName() << ", Tram " << arrival.tram->getID()
                 << " arrives at " << arrival.time.hour << ":" << arrival.time.minute << endl;
        }
        if(!stop->getArrivals().empty()){
            cout<<endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 5) {
        cout << "Usage: " << argv[0] << " <SIP_ADDR> <SIP_PORT> <SIP_NAME> <USER_PORT>" << endl;
        return 1;
    }

    try {
        Ice::CommunicatorHolder ich(argc, argv);
        auto adapter = ich->createObjectAdapterWithEndpoints("", "default -p " + string(argv[4]));
        auto sipProxy = Ice::checkedCast<SIPPrx>(ich->stringToProxy(
                string(argv[3]) + ":default -h " + argv[1] + " -p " + argv[2]));
        auto userProxy = Ice::checkedCast<UserPrx>(adapter->addWithUUID(make_shared<UserI>()));
        adapter->activate();

        while (true) {
            Ice::CtrlCHandler ctrlCHandler;
            cout << "Select option:\n0: Register to Stop\n1: Register to Tram\n";
            int choice;
            cin >> choice;
            if (cin.fail() || choice < 0 || choice > 1) {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                continue;
            }

            vector<TramPrxPtr> subscribed_trams;
            vector<StopPrxPtr> subscribed_stops;

            ctrlCHandler.setCallback([&userProxy, &ich, &subscribed_stops, &subscribed_trams] (int signal) {
                for(auto tram: subscribed_trams){
                    tram->unregisterUser(userProxy);
                }
                for(auto stop: subscribed_stops){
                    stop->unregisterUser(userProxy);
                }
                ich.communicator()->shutdown();
                quick_exit(0);
            });

            switch (choice) {
                case 0: { // Register to a stop
                    auto lines = sipProxy->getLines();
                    int maxID = 0;
                    for (auto& line : lines) {
                        cout << "Line stops:" << endl;
                        for (auto& stop : line->getStops()) {
                            cout << stop->getID() << ": " << stop->getName() << endl;
                            if(maxID < stop->getID()){
                                maxID = stop->getID();
                            }
                        }
                    }

                    int stopIndex;
                    cout << "Enter stop index to register: ";
                    cin >> stopIndex;

                    if (cin.fail() || stopIndex < 0 || stopIndex > maxID) {
                        cin.clear();
                        cin.ignore(numeric_limits<streamsize>::max(), '\n');
                        cout << "Invalid index, try again." << endl;
                        continue;
                    }

                    sipProxy->getStop(stopIndex)->registerUser(userProxy);
                    subscribed_stops.push_back(sipProxy->getStop(stopIndex));
                    cout << "Registered to stop: " << sipProxy->getStop(stopIndex)->getName() << endl;
                    break;
                }
                case 1: { // Register to a tram
                    auto lines = sipProxy->getLines();
                    vector<TramPrxPtr> trams;
                    int index = 0;
                    for (auto& line : lines) {
                        for (auto& tram : line->getTrams()) {
                            trams.push_back(tram);
                            cout << index++ << ": Tram ID " << tram->getID() << endl;
                        }
                    }
                    if(!trams.empty()){
                        int tramIndex;
                        cout << "Enter tram index to register: ";
                        cin >> tramIndex;

                        if (cin.fail() || tramIndex < 0 || tramIndex > index) {
                            cin.clear();
                            cin.ignore(numeric_limits<streamsize>::max(), '\n');
                            cout << "Invalid index, try again." << endl;
                            continue;
                        }

                        trams[tramIndex]->registerUser(userProxy);
                        cout << "Registered to tram ID: " << trams[tramIndex]->getID() << endl;
                        break;
                    }else{
                        cout << "no trams" <<endl;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    return 0;
}