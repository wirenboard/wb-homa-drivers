// TBD: make it possible to specify sysfs prefix
// TBD: proper error handling (exceptions)
// TBD: test with non-pre-exported gpios
// TBD: ensure proper slashes in sysfs dir
// TBD: config, channel names & types
// TBD: debug mode in config
// TBD: poll interval

#include <cstdlib>
#include <iostream>
#include <getopt.h>

#include <mosquittopp.h>

#include "common/utils.h"
#include "common/mqtt_wrapper.h"
#include "sysfs_adc.h"

namespace {
    std::string GetSysfsPrefix()
    {
        const char* prefix = getenv("WB_SYSFS_PREFIX");
        return prefix ? prefix : "/sys";
    }
}

class TMQTTADCHandler : public TMQTTWrapper
{
public:
    TMQTTADCHandler(const TMQTTADCHandler::TConfig& mqtt_config);

    void OnConnect(int rc);
    void OnMessage(const struct mosquitto_message *message);
    void OnSubscribe(int mid, int qos_count, const int *granted_qos);

    std::string GetChannelTopic(const TSysfsADCChannel& channel) const;
    void UpdateChannelValues();
private:
    TSysfsADC ADC;
    vector<TSysfsADCChannel> Channels;
};

TMQTTADCHandler::TMQTTADCHandler(const TMQTTADCHandler::TConfig& mqtt_config)
    : TMQTTWrapper(mqtt_config), ADC(GetSysfsPrefix())
{
    Channels.push_back(ADC.GetChannel("R1"));
    Channels.push_back(ADC.GetChannel("R2"));
    Channels.push_back(ADC.GetChannel("R3"));
    Channels.push_back(ADC.GetChannel("R4"));

	Connect();
}

void TMQTTADCHandler::OnConnect(int rc)
{
    cerr << "Connected with code " << rc << endl;

    if(rc != 0)
        return;

    string path = string("/devices/") + MQTTConfig.Id + "/meta/name";
    Publish(NULL, path, "ADC", 0, true);

    for (auto channel : Channels) {
        std::string topic = GetChannelTopic(channel);
        Publish(NULL, topic + "/meta/type", "text", 0, true);
    }
}

void TMQTTADCHandler::OnMessage(const struct mosquitto_message *)
{
    // NOOP
}

void TMQTTADCHandler::OnSubscribe(int, int, const int *)
{
    std::cerr << "Subscription succeeded." << std::endl;
}

std::string TMQTTADCHandler::GetChannelTopic(const TSysfsADCChannel& channel) const
{
    static string controls_prefix = std::string("/devices/") + MQTTConfig.Id + "/controls/";
    return controls_prefix + channel.GetName();
}

void TMQTTADCHandler::UpdateChannelValues()
{
    for (auto channel : Channels) {
        int value = channel.GetValue();
        std::cout << "channel: " << channel.GetName() << " value: " << value << std::endl;
        Publish(NULL, GetChannelTopic(channel), to_string(value), 0, true);
    }
}

int main(int argc, char **argv)
{
	class TMQTTADCHandler* mqtt_handler;
	int rc;
    TMQTTADCHandler::TConfig mqtt_config;
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

    mqtt_config.Id = "wb-adc";

	mqtt_handler = new TMQTTADCHandler(mqtt_config);

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
