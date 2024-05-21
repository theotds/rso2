#include <Ice/Ice.h>
#include <mpk.h>
#include <memory>
#include <vector>
#include <iostream>
#include <chrono>
#include <thread>

using namespace std;
using namespace MPK;

TramPrxPtr tram_proxy;

class TramI : public Tram {
    vector<UserPrxPtr> registered_users;
    int tramID;
    LinePrxPtr associatedLine;
    schedule tramSchedule;
    TimeOfDay departureTime;

public:

    StopPrxPtr getStop(const ::Ice::Current&) override {
        return !tramSchedule.empty() ? tramSchedule.front().stop : nullptr;
    }

    void startTram(int id, LinePrxPtr line, TimeOfDay startTime) {
        this->tramID = id;
        this->associatedLine = line;
        this->departureTime = startTime;
        line->addTram(tram_proxy);
    }

    TimeOfDay getStopTime(int stopID, const ::Ice::Current&) override {
        auto iter = find_if(tramSchedule.begin(), tramSchedule.end(),
                            [stopID](const ScheduleItem& item) { return item.stop->getID({}) == stopID; });
        return iter != tramSchedule.end() ? iter->time : TimeOfDay{-1, -1};
    }

    int getID(const ::Ice::Current&) override {
        return tramID;
    }

    schedule getSchedule(const ::Ice::Current&) override {
        return tramSchedule;
    }

    void registerUser(UserPrxPtr user, const ::Ice::Current&) override {
        registered_users.push_back(user);
    }

    void unregisterUser(UserPrxPtr user, const ::Ice::Current&) override {
        registered_users.erase(remove(registered_users.begin(), registered_users.end(), user), registered_users.end());
    }

    void createSchedule() {
        int minutesBetweenStops = 10;
        TimeOfDay time = departureTime;
        for (auto& stop : associatedLine->getStops()) {
            time = updateTime(time, minutesBetweenStops);
            tramSchedule.push_back(ScheduleItem{stop, time});
        }
    }

    TimeOfDay updateTime(TimeOfDay time, int increment) {
        time.minute += increment;
        time.hour += time.minute / 60;
        time.minute %= 60;
        time.hour %= 24;
        return time;
    }

    void start() {
        while (true){
            for(auto user:registered_users){
                try{
                    user->updateStop(tram_proxy,tramSchedule[0].stop);
                }catch(const std::exception& e){

                }
            }
            cout<<"Arrived at stop "<<tramSchedule[0].stop->getName()<<endl;
            tramSchedule.erase(tramSchedule.cbegin());

            if(tramSchedule.empty()){
                break;
            }
            this_thread::sleep_for(2s);
        }
        associatedLine->removeTram(tram_proxy);
    }

    void exitLine(){
        associatedLine->removeTram(tram_proxy);
    }

};

int main(int argc, char* argv[]) {
    Ice::CtrlCHandler ctrlCHandler;
    try {
        Ice::CommunicatorHolder ich(argc, argv);
        ctrlCHandler.setCallback([&ich](int) {
            ich.communicator()->destroy();
            quick_exit(0);
        });

        if (argc < 5) {
            cout << "Usage: " << argv[0] << " <Server Address> <Server Port> <Service Name> <Local Port>" << endl;
            return 1;
        }

        string serviceEndpoint = string("default -p ") + argv[4];
        auto adapter = ich->createObjectAdapterWithEndpoints("", serviceEndpoint);
        string proxyString = string(argv[3]) + ":default -h " + argv[1] + " -p " + argv[2];
        auto serviceProxy = Ice::checkedCast<SIPPrx>(ich->stringToProxy(proxyString));
        auto allLines = serviceProxy->getLines();

        int tramID;
        cout << "Enter tram ID: ";
        while (!(cin >> tramID) || tramID < 0) {
            cout << "Invalid tram ID, try again: ";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }

        size_t lineIndex;
        cout << "Available lines:\n";
        for (size_t i = 0; i < allLines.size(); ++i) {
            cout << i << ": ";
            for (const auto& stop : allLines[i]->getStops()) {
                cout << stop->getName() << " ";
            }
            cout << endl;
        }
        cout << "Select a line index: ";
        while (!(cin >> lineIndex) || lineIndex >= allLines.size()) {
            cout << "Invalid index, try again: ";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }

        TimeOfDay startTime;
        cout << "Set start time (HH MM): ";
        while (!(cin >> startTime.hour >> startTime.minute) ||
               startTime.hour < 0 || startTime.hour > 23 ||
               startTime.minute < 0 || startTime.minute > 59) {
            cout << "Invalid time, please enter HH MM format: ";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }

        auto tram = make_shared<TramI>();
        tram_proxy = Ice::checkedCast<TramPrx>(adapter->addWithUUID(tram));
        adapter->activate();

        tram->startTram(tramID, allLines[lineIndex], startTime);
        tram->createSchedule();

        ctrlCHandler.setCallback([&ich, &tram](int) {
            tram->exitLine();
            ich.communicator()->destroy();
            quick_exit(0);
        });

        cout << "Tram operation underway." << endl;
        tram->start();
        cout << "Tram operation completed." << endl;

        ich.communicator()->shutdown();
        quick_exit(0);
    } catch (const std::exception &e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    return 0;
}