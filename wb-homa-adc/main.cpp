// TBD: debug mode in config

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <getopt.h>

#include "jsoncpp/json/json.h"

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

struct THandlerConfig
{
    std::string DeviceName = "ADCs";
    bool Debug = false;
    int AveragingWindow = 10;
    int MinSwitchIntervalMs = 0;
};

class TMQTTADCHandler : public TMQTTWrapper
{
public:
    TMQTTADCHandler(const TMQTTADCHandler::TConfig& mqtt_config, const THandlerConfig& handler_config);

    void OnConnect(int rc);
    void OnMessage(const struct mosquitto_message *message);
    void OnSubscribe(int mid, int qos_count, const int *granted_qos);

    std::string GetChannelTopic(const TSysfsADCChannel& channel) const;
    void UpdateChannelValues();
private:
    THandlerConfig Config;
    TSysfsADC ADC;
    vector<TSysfsADCChannel> Channels;
};

TMQTTADCHandler::TMQTTADCHandler(const TMQTTADCHandler::TConfig& mqtt_config, const THandlerConfig& handler_config)
    : TMQTTWrapper(mqtt_config),
      Config(handler_config),
      ADC(GetSysfsPrefix(),
          handler_config.AveragingWindow,
          handler_config.MinSwitchIntervalMs,
          handler_config.Debug)
{
    for (int i = 0; i < 8; ++i)
        Channels.push_back(ADC.GetChannel("ADC" + std::to_string(i)));

	Connect();
}

void TMQTTADCHandler::OnConnect(int rc)
{
    if (Config.Debug)
        std::cerr << "Connected with code " << rc << std::endl;

    if(rc != 0)
        return;

    string path = string("/devices/") + MQTTConfig.Id + "/meta/name";
    Publish(NULL, path, Config.DeviceName.c_str(), 0, true);

    int n = 0;
    for (auto channel : Channels) {
        std::string topic = GetChannelTopic(channel);
        Publish(NULL, topic + "/meta/type", "text", 0, true);
        Publish(NULL, topic + "/meta/order", std::to_string(n++), 0, true);
    }
}

void TMQTTADCHandler::OnMessage(const struct mosquitto_message *)
{
    // NOOP
}

void TMQTTADCHandler::OnSubscribe(int, int, const int *)
{
    if (Config.Debug)
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
        if (Config.Debug)
            std::cerr << "channel: " << channel.GetName() << " value: " << value << std::endl;
        Publish(NULL, GetChannelTopic(channel), to_string(value), 0, true);
    }
}

namespace {
    void LoadConfig(const std::string& file_name, THandlerConfig& config)
    {
        ifstream config_file (file_name);
        if (config_file.fail())
            return; // just use defaults

        Json::Value root;
        Json::Reader reader;
        bool parsedSuccess = reader.parse(config_file, root, false);

        // Report failures and their locations in the document.
        if(not parsedSuccess)
            throw TADCException("Failed to parse config JSON: " + reader.getFormatedErrorMessages());

        if (!root.isObject())
            throw TADCException("Bad config file (the root is not an object)");

        if (root.isMember("debug"))
            config.Debug = root["debug"].asBool();

        if (root.isMember("device_name"))
            config.DeviceName = root["device_name"].asString();

        if (root.isMember("averaging_window")) {
            config.AveragingWindow = root["averaging_window"].asInt();
            if (config.AveragingWindow < 1)
                throw TADCException("bad averaging window specified in the config");
        }

        if (root.isMember("min_switch_interval_ms"))
            config.MinSwitchIntervalMs = root["min_switch_interval_ms"].asInt();
    }
};

int main(int argc, char **argv)
{
	class TMQTTADCHandler* mqtt_handler;
	int rc;
    string config_fname;
    bool debug = false;
    TMQTTADCHandler::TConfig mqtt_config;
    mqtt_config.Host = "localhost";
    mqtt_config.Port = 1883;

    int c;
    //~ int digit_optind = 0;
    //~ int aopt = 0, bopt = 0;
    //~ char *copt = 0, *dopt = 0;
    while ( (c = getopt(argc, argv, "dc:h:p:")) != -1) {
        //~ int this_option_optind = optind ? optind : 1;
        switch (c) {
        case 'd':
            debug = true;
            break;
        case 'c':
            config_fname = optarg;
            break;
        //~ case 'c':
            //~ printf ("option c with value '%s'\n", optarg);
            //~ config_fname = optarg;
            //~ break;
        case 'p':
            mqtt_config.Port = stoi(optarg);
            break;
        case 'h':
            mqtt_config.Host = optarg;
            break;
        case '?':
            break;
        default:
            printf ("?? getopt returned character code 0%o ??\n", c);
        }
    }
	mosqpp::lib_init();

    try {
        THandlerConfig config;
        if (!config_fname.empty())
            LoadConfig(config_fname, config);

        config.Debug = config.Debug || debug;
        mqtt_config.Id = "wb-adc";
        mqtt_handler = new TMQTTADCHandler(mqtt_config, config);

        while(1){
            rc = mqtt_handler->loop();
            if(rc != 0)
                mqtt_handler->reconnect();
            else // update current values
                mqtt_handler->UpdateChannelValues();
        }
    } catch (const TADCException& e) {
        std::cerr << "FATAL: " << e.what() << std::endl;
        return 1;
    }

	mosqpp::lib_cleanup();

	return 0;
}
