#include <cstdlib>
#include <iostream>
#include <getopt.h>

#include "jsoncpp/json/json.h"

#include "adc_handler.h"

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
        
        if (root.isMember("averaging_window")) {
            config.AveragingWindow = root["averaging_window"].asInt();
            if (config.AveragingWindow < 1)
                throw TADCException("bad averaging window specified in the config");
        }

        const auto& array = root["iio_channels"];

        for (unsigned int index = 0; index < array.size(); index++){
            const auto& item = array[index];
        }
                        
        /*if (!root.isObject())
            throw TADCException("Bad config file (the root is not an object)");

        if (root.isMember("debug"))
            config.Debug = root["debug"].asBool();

        if (root.isMember("device_name"))
            config.DeviceName = root["device_name"].asString();

        

        if (root.isMember("min_switch_interval_ms"))
            config.MinSwitchIntervalMs = root["min_switch_interval_ms"].asInt();
            */

        if (root.isMember("id")) 
            config.Id = root["id"].asString();
        if (root.isMember("multiplier"))
            config.Multiplier = root["multiplier"].asFloat();
        if (root.isMember("gpio"))
            config.Gpio = root["gpio"].asInt();
    }
};

int main(int argc, char **argv)
{
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
        std::shared_ptr<TMQTTADCHandler> mqtt_handler(new TMQTTADCHandler(mqtt_config, config));
        mqtt_handler->Init();

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
