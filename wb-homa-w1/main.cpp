#include <iostream>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#include "dirent.h"
#include <getopt.h>
#include <chrono>
// This is the JSON header
#include "jsoncpp/json/json.h"

#include <mosquittopp.h>

#include <utils.h>
#include <mqtt_wrapper.h>

#include "sysfs_w1.h"

using namespace std;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::steady_clock;

class TMQTTOnewireHandler : public TMQTTWrapper
{
    //~ typedef pair<TGpioDesc, TSysfsGpio> TChannelDesc;

	public:
        TMQTTOnewireHandler(const TMQTTOnewireHandler::TConfig& mqtt_config);
		~TMQTTOnewireHandler();

		void OnConnect(int rc);
		void OnMessage(const struct mosquitto_message *message);
		void OnSubscribe(int mid, int qos_count, const int *granted_qos);

        inline string GetChannelTopic(const TSysfsOnewireDevice& device);
        void RescanBus();

        void UpdateChannelValues();
        inline void SetPrepareInit(bool b) { PrepareInit = b; }// changing bool PrepareInit
        inline bool GetPrepareInit() { return PrepareInit; }
    private:
        vector<TSysfsOnewireDevice> Channels;
        bool PrepareInit;// needed for cleaning mqtt messages before start working
        string Retained_old;// we need some message to be sure, that we got all retained messages in starting

};




TMQTTOnewireHandler::TMQTTOnewireHandler(const TMQTTOnewireHandler::TConfig& mqtt_config)
    : TMQTTWrapper(mqtt_config)
    , PrepareInit(true)
{
	Connect();

    Retained_old = string("/tmp/") + MQTTConfig.Id + "/retained_old";

}

TMQTTOnewireHandler::~TMQTTOnewireHandler() {}

void TMQTTOnewireHandler::OnConnect(int rc)
{
	printf("Connected with code %d.\n", rc);
	if(rc == 0){
                // Meta
        string path = string("/devices/") + MQTTConfig.Id + "/meta/name";
        Publish(NULL, path, "1-wire Thermometers", 0, true);


        if (PrepareInit){
            string controls = string("/devices/") + MQTTConfig.Id + "/controls/+";
            Subscribe(NULL, controls);
            Subscribe(NULL, Retained_old);
            Publish(NULL, Retained_old, "1", 0, false);
         }else{
            RescanBus();
         }
	}
}

template<typename T>
void UnorderedVectorDifference(const vector<T> &first, const vector<T>& second, vector<T> & result)
{
    for (auto & el_first: first) {
        bool found = false;
        for (auto & el_second: second) {
            if (el_first == el_second) {
                found = true;
                break;
            }
        }

        if (!found) {
            result.push_back(el_first);
        }
    }
}

void TMQTTOnewireHandler::RescanBus()
{
    vector<TSysfsOnewireDevice> current_channels;

    DIR *dir;
    struct dirent *ent;
    string entry_name;
    if ((dir = opendir (SysfsOnewireDevicesPath.c_str())) != NULL) {
        /* print all the files and directories within directory */
        while ((ent = readdir (dir)) != NULL) {
            printf ("%s\n", ent->d_name);
            entry_name = ent->d_name;
            if (StringStartsWith(entry_name, "28-") or StringStartsWith(entry_name, "10-")) {
                    current_channels.emplace_back(entry_name);
            }
        }
        closedir (dir);
    } else {
        cerr << "ERROR: could not open directory " << SysfsOnewireDevicesPath << endl;
    }

    vector<TSysfsOnewireDevice> new_channels;
    UnorderedVectorDifference(current_channels, Channels, new_channels);

    vector<TSysfsOnewireDevice> absent_channels;
    UnorderedVectorDifference(Channels, current_channels, absent_channels);


    Channels.swap(current_channels);

    for (const TSysfsOnewireDevice& device: new_channels) {
        Publish(NULL, GetChannelTopic(device) + "/meta/type", "temperature", 0, true);
    }

    //delete retained messages for absent channels
    for (const TSysfsOnewireDevice& device: absent_channels) {
        Publish(NULL, GetChannelTopic(device) + "/meta/type", "", 0, true);
        Publish(NULL, GetChannelTopic(device), "", 0, true);
    }
}

void TMQTTOnewireHandler::OnMessage(const struct mosquitto_message *message)
{
    string topic = message->topic;
    string controls_prefix = string("/devices/") + MQTTConfig.Id + "/controls/";
    if (topic == Retained_old) {// if we get old_message it means that we've read all retained messages
        Publish(NULL, Retained_old, "", 0, true);
        unsubscribe(NULL, Retained_old.c_str());
        unsubscribe(NULL, (controls_prefix + "+").c_str());
        PrepareInit = false;
    }else {
        string device = topic.substr(controls_prefix.length(), topic.length());
        for (auto& current : Channels)
            if (device == current.GetDeviceId())
                return;
        Channels.emplace_back(device);
    }

}

void TMQTTOnewireHandler::OnSubscribe(int mid, int qos_count, const int *granted_qos)
{
	printf("Subscription succeeded.\n");
}

string TMQTTOnewireHandler::GetChannelTopic(const TSysfsOnewireDevice& device) {
    static string controls_prefix = string("/devices/") + MQTTConfig.Id + "/controls/";
    return (controls_prefix + device.GetDeviceId());
}

void TMQTTOnewireHandler::UpdateChannelValues() {

    for (const TSysfsOnewireDevice& device: Channels) {
        auto result = device.ReadTemperature();
        if (result.Defined()) {
            Publish(NULL, GetChannelTopic(device), to_string(*result), 0, true); // Publish current value (make retained)
        }

    }
}


int main(int argc, char *argv[])
{
	int rc;
    TMQTTOnewireHandler::TConfig mqtt_config;
    mqtt_config.Host = "localhost";
    mqtt_config.Port = 1883;
    int poll_interval = 10 * 1000; //milliseconds

    int c;
    //~ int digit_optind = 0;
    //~ int aopt = 0, bopt = 0;
    //~ char *copt = 0, *dopt = 0;
    while ( (c = getopt(argc, argv, "c:h:p:i:")) != -1) {
        //~ int this_option_optind = optind ? optind : 1;
        switch (c) {
        //~ case 'c':
            //~ printf ("option c with value '%s'\n", optarg);
            //~ config_fname = optarg;
            //~ break;
        case 'p':
            printf ("option p with value '%s'\n", optarg);
            mqtt_config.Port = stoi(optarg);
            break;
        case 'h':
            printf ("option h with value '%s'\n", optarg);
            mqtt_config.Host = optarg;
            break;
        case 'i':
			poll_interval = stoi(optarg) * 1000;
        case '?':
            break;
        default:
            printf ("?? getopt returned character code 0%o ??\n", c);
        }
    }
	mosqpp::lib_init();

    mqtt_config.Id = "wb-w1";

    std::shared_ptr<TMQTTOnewireHandler> mqtt_handler(new TMQTTOnewireHandler(mqtt_config));
    mqtt_handler->Init();
    string topic = string("/devices/") + mqtt_config.Id + "/controls/+";

	auto time_last_published = steady_clock::now();
    while(1){
		rc = mqtt_handler->loop(poll_interval);
        //~ cout << "break in a loop! " << rc << endl;
		if(rc != 0) {
			mqtt_handler->reconnect();
		} else {
            // update current values
            if (!mqtt_handler->GetPrepareInit()){
				int time_elapsed = duration_cast<milliseconds>(steady_clock::now() - time_last_published).count() ;
				if (time_elapsed >= poll_interval ) { //checking is it time to look through all gpios
	                mqtt_handler->RescanBus();
	                mqtt_handler->UpdateChannelValues();
	                time_last_published = steady_clock::now();
				}
            }
        }
	}

	mosqpp::lib_cleanup();

	return 0;
}
