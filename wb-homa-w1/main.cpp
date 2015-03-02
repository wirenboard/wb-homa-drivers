#include <iostream>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#include "dirent.h"
#include <getopt.h>

// This is the JSON header
#include "jsoncpp/json/json.h"

#include <mosquittopp.h>

#include "common/utils.h"
#include "common/mqtt_wrapper.h"

#include "sysfs_w1.h"

using namespace std;


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
    private:
        vector<TSysfsOnewireDevice> Channels;
        bool PrepareInit;// needed for cleaning mqtt messages before start working

};




TMQTTOnewireHandler::TMQTTOnewireHandler(const TMQTTOnewireHandler::TConfig& mqtt_config)
    : TMQTTWrapper(mqtt_config)
    , PrepareInit(true)
{
	Connect();

}

TMQTTOnewireHandler::~TMQTTOnewireHandler() {}

void TMQTTOnewireHandler::OnConnect(int rc)
{
	printf("Connected with code %d.\n", rc);
    if (PrepareInit) 
        return;
	if(rc == 0){
                // Meta
        string path = string("/devices/") + MQTTConfig.Id + "/meta/name";
        Publish(NULL, path, "1-wire Thermometers", 0, true);

        RescanBus();
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
    string device = topic.substr(controls_prefix.length(), topic.length());
    for (auto& current : Channels)
        if (device.substr(3, 3 + 6*2) == current.GetDeviceId()) 
            return;
    device = "BUF" + device;// such as it's not real device and we need 3 symbols before device.Id, add some random symbols
    Channels.emplace_back(device);

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

    int c;
    //~ int digit_optind = 0;
    //~ int aopt = 0, bopt = 0;
    //~ char *copt = 0, *dopt = 0;
    while ( (c = getopt(argc, argv, "c:h:p:")) != -1) {
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
	int Number = 3;
    string topic = string("/devices/") + mqtt_config.Id + "/controls/+";
    int i;
	mqtt_handler->Subscribe(NULL,topic);
	for (i = 0; i< Number; i++){// waiting in a loop while all retained messages comes 
		rc = mqtt_handler->loop();
		if (rc != 0){
			mqtt_handler->reconnect();
		    mqtt_handler->Subscribe(NULL,topic);
		}
    }
    mqtt_handler->unsubscribe(NULL, topic.c_str());
    mqtt_handler->SetPrepareInit(false);
	
	while(1){
		rc = mqtt_handler->loop();
        //~ cout << "break in a loop! " << rc << endl;
		if(rc != 0) {
			mqtt_handler->reconnect();
		} else {
            // update current values
            mqtt_handler->RescanBus();
            mqtt_handler->UpdateChannelValues();
        }
	}

	mosqpp::lib_cleanup();

	return 0;
}
