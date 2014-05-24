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

        void UpdateChannelValues();
    private:
        vector<TSysfsOnewireDevice> Channels;

};







TMQTTOnewireHandler::TMQTTOnewireHandler(const TMQTTOnewireHandler::TConfig& mqtt_config)
    : TMQTTWrapper(mqtt_config)
{
	Connect();


    DIR *dir;
    struct dirent *ent;
    string entry_name;
    if ((dir = opendir (SysfsOnewireDevicesPath.c_str())) != NULL) {
        /* print all the files and directories within directory */
        while ((ent = readdir (dir)) != NULL) {
            printf ("%s\n", ent->d_name);
            entry_name = ent->d_name;
            if (StringStartsWith(entry_name, "28-")) {
                Channels.emplace_back(entry_name);
            }
        }
        closedir (dir);
    } else {
        /* could not open directory */
        //~ perror ("");
        //~ return EXIT_FAILURE;
    }


    //~ Channels.emplace_back("28-000004a76ec4");
    //~ Channels.emplace_back("28-000004d11d72");

};

void TMQTTOnewireHandler::OnConnect(int rc)
{
	printf("Connected with code %d.\n", rc);
	if(rc == 0){

                // Meta
        string path = string("/devices/") + MQTTConfig.Id + "/meta/name";
        Publish(NULL, path, "1-wire Thermometers", 0, true);

        for (const TSysfsOnewireDevice& device: Channels) {
            Publish(NULL, GetChannelTopic(device) + "/meta/type", "temperature", 0, true);
        }



	}
}

void TMQTTOnewireHandler::OnMessage(const struct mosquitto_message *message)
{
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
    //~ for (TChannelDesc& channel_desc : Channels) {
        //~ auto & gpio_desc = channel_desc.first;
        //~ auto & gpio_handler = channel_desc.second;
//~
        //~ // vv order matters
        //~ int cached = gpio_handler.GetCachedValue();
        //~ int value = gpio_handler.GetValue();
//~
        //~ if (value >= 0) {
            //~ if ( (cached < 0) || (cached != value)) {
                //~ Publish(NULL, GetChannelTopic(gpio_desc), to_string(value), 0, true); // Publish current value (make retained)
            //~ }
        //~ }
    //~ }
//~
}


int main(int argc, char *argv[])
{
	class TMQTTOnewireHandler* mqtt_handler;
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
    //~ if (optind < argc) {
        //~ printf ("non-option ARGV-elements: ");
        //~ while (optind < argc)
            //~ printf ("%s ", argv[optind++]);
        //~ printf ("\n");
    //~ }



	mosqpp::lib_init();

    mqtt_config.Id = "wb-w1";

	mqtt_handler = new TMQTTOnewireHandler(mqtt_config);

	while(1){
		rc = mqtt_handler->loop();
        //~ cout << "break in a loop! " << rc << endl;
		if(rc != 0) {
			mqtt_handler->reconnect();
		} else {
            // update current values
            mqtt_handler->UpdateChannelValues();
        }
	}

	mosqpp::lib_cleanup();

	return 0;
}
