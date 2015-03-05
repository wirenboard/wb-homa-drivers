#include<iostream>
#include"adc_handler.h"

using namespace std;

namespace {
    std::string GetSysfsPrefix()
    {
        const char* prefix = getenv("WB_SYSFS_PREFIX");
        return prefix ? prefix : "/sys";
    }
}

vector<std::shared_ptr<TMQTTADCHandler>> TMQTTADCHandler::GetADCHandler(const TMQTTADCHandler::TConfig& mqtt_config, const THandlerConfig& handler_config){
    vector<std::shared_ptr<TMQTTADCHandler>> buf_vector;
    for (auto& channel: handler_config.Channels){// search for mux channel
        if (channel.Type == "mux") {
            std::shared_ptr<TMQTTADCHandlerMux> mqtt_handler(new TMQTTADCHandlerMux(mqtt_config, handler_config.DeviceName, handler_config.Debug, channel));
            buf_vector.push_back(mqtt_handler);
        }else 
            buf_vector.push_back(nullptr);
    }
    return buf_vector;
}

TMQTTADCHandler::TMQTTADCHandler(const TMQTTADCHandler::TConfig& mqtt_config, const string& device_name, bool debug)
    : TMQTTWrapper(mqtt_config),
    DeviceName(device_name),
    Debug(debug)
{
    GeneralPublish = false;
}

bool TMQTTADCHandler::GeneralPublish = false;

TMQTTADCHandlerMux::TMQTTADCHandlerMux(const TMQTTADCHandler::TConfig& mqtt_config,const string& device_name, bool debug, const TChannel& channel_config)
    : TMQTTADCHandler(mqtt_config, device_name, debug)
{
    ADC.reset(new TSysfsADC(GetSysfsPrefix(),channel_config.AveragingWindow, channel_config.MinSwitchIntervalMs, debug, channel_config.Gpios, channel_config.Mux));

            
    for (int i = 0; i < 8; ++i)
        Channels.push_back(ADC->GetChannel(i));
	Connect();
}

void TMQTTADCHandlerMux::OnConnect(int rc)
{
    if (Debug)
        std::cerr << "Connected with code " << rc << std::endl;

    if(rc != 0)
        return;

    if (!GeneralPublish) {
        string path = string("/devices/") + MQTTConfig.Id + "/meta/name";
        Publish(NULL, path, DeviceName.c_str(), 0, true);
        //GeneralPublish = true;
    }

    int n = 0;
    for (auto channel : Channels) {
        std::string topic = GetChannelTopic(channel);
        //Publish(NULL, topic + "/meta/type", "text", 0, true);
        Publish(NULL, topic + "/meta/order", std::to_string(n++), 0, true);
        Publish(NULL, topic + "/meta/type", "voltage", 0, true);
    }
}

void TMQTTADCHandlerMux::OnMessage(const struct mosquitto_message *)
{
    // NOOP
}

void TMQTTADCHandlerMux::OnSubscribe(int, int, const int *)
{
    if (Debug)
        std::cerr << "Subscription succeeded." << std::endl;
}

string TMQTTADCHandlerMux::GetChannelTopic(const TSysfsADCChannel& channel) const
{
    static string controls_prefix = std::string("/devices/") + MQTTConfig.Id + "/controls/";
    return controls_prefix + channel.GetName();
}

void TMQTTADCHandlerMux::UpdateValue(){
    UpdateChannelValues();
}
void TMQTTADCHandlerMux::UpdateChannelValues()
{
    for (auto channel : Channels) {
        int value = channel.GetValue();
        if (Debug)
            std::cerr << "channel: " << channel.GetName() << " value: " << value << std::endl;
        float result = channel.GetMultiplier() * value;

        Publish(NULL, GetChannelTopic(channel), to_string(result), 0, true);
    }
}
