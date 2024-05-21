#include <Ice/Ice.h>
#include <mpk.h>
#include <memory>
#include <vector>
#include <Ice/Initialize.h>
#include <thread>
#include <Ice/Proxy.h>
#include <algorithm>

using namespace std;
using namespace MPK;

int next_stop_id = 0;
SIPPrxPtr sip_proxy;

class StopI : public Stop {
    string name;
    int id;
    vector<UserPrxPtr> users;

public:
    StopI(string name) : name(name) {this->id = next_stop_id++;};

    lines getLines(const ::Ice::Current& current){
        lines city_lines;
        for(auto line: sip_proxy->getLines()){
            auto line_stops = line->getStops();
            auto pos = find_if(line_stops.cbegin(), line_stops.cend(), [&current](StopPrxPtr s){return s->ice_getIdentity() == current.id;});
            if(pos != line_stops.cend()){
                city_lines.push_back(line);
            }
        }
        return city_lines;
    }

    int getID(const Ice::Current &) { return id; }

    string getName(const Ice::Current &) { return name; }

    arrivals getArrivals(const ::Ice::Current &current) {
        arrivals upcomingArrivals;

        auto activeLines = getLines(current);

        for (const auto &line: activeLines) {
            auto tramsOnLine = line->getTrams();

            // Check each tram's schedule to see if it stops at the current stop
            for (const auto &tram: tramsOnLine) {
                auto tramSchedule = tram->getSchedule();

                // Search through the tram's schedule
                for (const auto &scheduleItem: tramSchedule) {
                    if (scheduleItem.stop->ice_getIdentity() == current.id) {
                        upcomingArrivals.push_back(ArrivalInfoItem{tram, scheduleItem.time});
                    }
                }
            }
        }

        return upcomingArrivals;
    }

    void registerUser(UserPrxPtr user, const ::Ice::Current& current){
        users.push_back(user);
    }

    void unregisterUser(UserPrxPtr user, const ::Ice::Current& current){
        auto it = std::remove_if(users.begin(), users.end(), [&user](UserPrxPtr ptr) { return ptr->ice_getIdentity() == user->ice_getIdentity();});
        if (it != users.end())
        {
            users.erase(it);
            cout<<"User Left"<<endl;
        }
    }
    void updateUsers(StopPrxPtr stopPrx){
        for(auto user:users){
            try{
                user->updateSchedule(stopPrx, stopPrx->getArrivals());
            }catch(const std::exception& e){
                continue;
            }
        }
    }
};

class LineI : public Line {
    stops line_stops;
    trams line_trams;

public:
    LineI(vector <StopPrxPtr> stops) : line_stops(stops) {};

    trams getTrams(const Ice::Current &) override {
        return line_trams; // Return the list of trams on this line
    }

    stops getStops(const Ice::Current &) override {
        return line_stops; // Now correctly returning a vector of shared_ptr<StopPrx>
    }

    void addTram(TramPrxPtr tram, const ::Ice::Current &current) {
        line_trams.push_back(tram);
        cout << "Tram added" << endl;
    }

    void removeTram(TramPrxPtr tram, const ::Ice::Current &current) {
        for (auto it = line_trams.begin(); it != line_trams.end(); ++it) {
            if ((*it)->ice_getIdentity() == tram->ice_getIdentity()) {
                line_trams.erase(it);
                cout << "Tram removed" << endl;
                break; // Exit the loop after removing the tram to prevent invalidating the iterator
            }
        }
    }
};


class SIPI : public SIP {
    lines sip_lines;

public:

    lines getLines(const ::Ice::Current &current) {
        return sip_lines;
    }

    void addLine(LinePrxPtr line, const Ice::Current &) {
        sip_lines.push_back(line);
    }

    void removeLine(LinePrxPtr line, const Ice::Current &) {
        auto it = std::find_if(sip_lines.begin(), sip_lines.end(),
                               [&line](const LinePrxPtr &l) { return l.get() == line.get(); });
        if (it != sip_lines.end()) {
            sip_lines.erase(it);
            cout << "Line removed" << endl;
        }
    }


    StopPrxPtr getStop(int ID, const ::Ice::Current& current) {
        for(auto line: sip_lines){
            for(auto stop: line->getStops()){
                if(stop->getID() == ID){
                    return stop;
                }
            }
        }
        return NULL;
    }
};

void lineConfig(const stops &stop_proxies, stops &line_stops, int size, int maxID) {
    for(int i = 0; i<size;i++){
        line_stops.push_back(stop_proxies[rand() % maxID]);
    }
}

int main(int argc, char *argv[]) {
    try {

        Ice::CommunicatorHolder ich(argc, argv);
        if (argc < 3) {
            cout << "Nieprawidowa skladnia: <SIP_PORT> <SIP_NAME>" << endl;
            return 1;
        }
        auto adapter = ich->createObjectAdapterWithEndpoints("", string("default -p ").append(argv[1]));
        sip_proxy = Ice::checkedCast<SIPPrx>(adapter->add(make_shared<SIPI>(), Ice::stringToIdentity(argv[2])));
        auto stop_names = {"a", "b", "c", "d", "e"};
        vector <shared_ptr<StopI>> line_stops;
        for (auto stop_name: stop_names) {
            line_stops.push_back(make_shared<StopI>(stop_name));
        }
        stops stop_proxies;
        for (auto stop: line_stops) {
            auto stop_proxy = Ice::checkedCast<StopPrx>(adapter->addWithUUID(stop));
            stop_proxies.push_back(stop_proxy);
        }

        stops line1_stops;
        lineConfig(stop_proxies,line1_stops,5,5);
        auto line1_proxy = Ice::checkedCast<LinePrx>(adapter->addWithUUID(make_shared<LineI>(line1_stops)));
        sip_proxy->addLine(line1_proxy);

        stops line2_stops;
        lineConfig(stop_proxies,line2_stops,6,5);
        auto line2_proxy = Ice::checkedCast<LinePrx>(adapter->addWithUUID(make_shared<LineI>(line2_stops)));
        sip_proxy->addLine(line2_proxy);

        stops line3_stops;
        lineConfig(stop_proxies,line3_stops,4,5);
        auto line3_proxies = Ice::checkedCast<LinePrx>(adapter->addWithUUID(make_shared<LineI>(line3_stops)));
        sip_proxy->addLine(line3_proxies);

        auto lines = sip_proxy->getLines();
        int lineIndex = 1;

        for (const auto& line : lines) {
            auto stops = line->getStops();
            cout << "Line " << lineIndex << ": ";
            for (auto it = stops.begin(); it != stops.end(); ++it) {
                cout << (*it)->getName();
                if (next(it) != stops.end()) {
                    cout << " -> ";
                }
            }
            cout << endl;
            lineIndex++;
        }

        adapter->activate();
        cout << "Server started..." << endl;
        while (true) {
            for (int iter = 0; iter < line_stops.size(); iter++) {
                line_stops[iter]->updateUsers(stop_proxies[iter]);
                this_thread::sleep_for(5s);
            }
        }
    }
    catch (const std::exception &e) {
        cerr << e.what() << endl;
        return 1;
    }
    return 0;
}