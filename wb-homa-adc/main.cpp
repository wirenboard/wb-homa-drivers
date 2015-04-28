#include <cstdlib>
#include <iostream>
#include <getopt.h>
#include <string>
#include "jsoncpp/json/json.h"

#include "adc_handler.h"

namespace {
    int ReadResistance(std::string res)
    {
        std::string ohm;
        unsigned int resistance,i;
        for (i = 0; i < res.size();i++)
            if (res[i] >'9' || res[i] < '0')
                break;
        if (i == 0 ) {
            cerr << "incorrect resistance\n";
            exit(-1);
        }
        resistance = std::stoi(res);
        ohm = res.substr(i);
        if ( ohm == "MOhm")
            resistance *= 1000000;
        if ( ohm == "kOhm")
                resistance *= 1000;
        return resistance;
    }

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
            throw TAdcException("Failed to parse config JSON: " + reader.getFormatedErrorMessages());
        if (!root.isObject())
            throw TAdcException("Bad config file (the root is not an object)");
        if (root.isMember("device_name"))
            config.DeviceName = root["device_name"].asString();
        if (root.isMember("debug"))
                config.Debug = root["debug"].asBool();



             const auto& array = root["iio_channels"];

        for (unsigned int index = 0; index < array.size(); index++){
            const auto& item = array[index];
            TChannel new_channel;// create new intrance to add in vector<TChannel> Channels

            if (item.isMember("averaging_window")) {
                new_channel.AveragingWindow = item["averaging_window"].asInt();
                if (new_channel.AveragingWindow < 1)
                    throw TAdcException("bad averaging window specified in the config");
            }
            if (item.isMember("max_voltage")) {
                new_channel.MaxVoltage = 1000 * item["max_voltage"].asFloat();
            }
            if (item.isMember("id")) {
                TMUXChannel buf_channel;
                buf_channel.Id = item["id"].asString();
                if (item.isMember("multiplier"))
                    buf_channel.Multiplier = item["multiplier"].asFloat();
                if (item.isMember("readings_number")) {
                    buf_channel.ReadingsNumber = item["readings_number"].asInt();
                }
                if (item.isMember("decimal_places")) {
                    buf_channel.DecimalPlaces = item["decimal_places"].asInt();
                }
                if (item.isMember("discharge_channel")) {
                    buf_channel.DischargeChannel = item["discharge_channel"].asInt();
                }
                new_channel.Mux.push_back(buf_channel);
            }

            if (item.isMember("min_switch_interval_ms"))
                new_channel.MinSwitchIntervalMs = item["min_switch_interval_ms"].asInt();
            if (item.isMember("channel_number"))
                new_channel.ChannelNumber = item["channel_number"].asInt();

            if ( item.isMember("channels")){
                const auto& channel_array = item["channels"];
                if (channel_array.size() != 8) {
                    cerr << "Warning: number of mux channels is not equal to  8 " << endl;
                }

                if (item.isMember("gpios")){
                        const auto& gpios_array = item["gpios"];
                        if (gpios_array.size() != 3) {
                            cerr << "number of gpios isn't equal to  3" << endl;
                            exit(-1);
                        }
                        for (unsigned int i = 0; i< gpios_array.size(); i++){
                            const auto& gpio_item = gpios_array[i];
                            new_channel.Gpios.push_back(gpio_item.asInt());
                        }
                }

                for (unsigned int channel_number = 0; channel_number < channel_array.size(); channel_number++){
                    const auto& channel_iterator = channel_array[channel_number];
                    TMUXChannel element;
                    if (channel_iterator.isMember("readings_number")) {
                        element.ReadingsNumber = channel_iterator["readings_number"].asInt();
                    }
                    if (channel_iterator.isMember("decimal_places")) {
                        element.DecimalPlaces = channel_iterator["decimal_places"].asInt();
                    }
                    if (channel_iterator.isMember("mux_channel_number")) {
                        element.MuxChannelNumber = channel_iterator["mux_channel_number"].asInt();
                    }
                    if (channel_iterator.isMember("id"))
                        element.Id = channel_iterator["id"].asString();
                    if (channel_iterator.isMember("multiplier"))
                        element.Multiplier = channel_iterator["multiplier"].asFloat();
                    if (channel_iterator.isMember("type"))
                        element.Type = channel_iterator["type"].asString();
                    if (channel_iterator.isMember("current")){
                        int current = channel_iterator["current"].asInt();
                        if ( (current < 0) || (current > 300) || ( (current % 20) != 0)) {
                            cerr << "Error: wrong current value \n";
                            exit(EXIT_FAILURE);
                        }
                        element.Current = current;
                    }
                    if (channel_iterator.isMember("resistance1")){
                        int resistance = ReadResistance(channel_iterator["resistance1"].asString());
                        element.Resistance1 = resistance;
                    }
                    if (channel_iterator.isMember("resistance2")){
                        int resistance = ReadResistance(channel_iterator["resistance2"].asString());
                        element.Resistance2 = resistance;
                    }
                    if (channel_iterator.isMember("discharge_channel")) {
                        element.DischargeChannel = channel_iterator["discharge_channel"].asInt();
                    }
                    new_channel.Mux.push_back(element);
                }
                new_channel.Type = "mux";
            }
            config.Channels.push_back(new_channel);
        }
    }
};

int main(int argc, char **argv)
{
	int rc;
    string config_fname;
    bool debug = false;
    TMQTTAdcHandler::TConfig mqtt_config;
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
        /*for ( auto& i: config.Channels){
            cout << "AVERAGE IS " << i.AveragingWindow << endl;
            cout << "MINSWITCHINTERVAL IS " << i.MinSwitchIntervalMs << endl;
            cout << "Type IS " << i.Type << endl;
            if (i.Type == "mux" )
                cout << "MUX " << endl;
            for (auto& j : i.Mux){
                cout << "ID IS " << j.Id << endl;
                cout << "Multiplier IS " << j.Multiplier << endl;
            }
            for (auto& j : i.Gpios){
                cout << "GPIO IS " << j << endl;
            }
        }*/
        std::shared_ptr<TMQTTAdcHandler> mqtt_handler( new TMQTTAdcHandler(mqtt_config, config));
        mqtt_handler->Init();
        while(1){
                rc = mqtt_handler->loop();
                if(rc != 0)
                    mqtt_handler->reconnect();
                else // update current values
                    mqtt_handler->UpdateValue();
        }
    } catch (const TAdcException& e) {
        std::cerr << "FATAL: " << e.what() << std::endl;
        return 1;
    }

	mosqpp::lib_cleanup();

	return 0;
}
