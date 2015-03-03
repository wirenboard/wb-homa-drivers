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

std::shared_ptr<TMQTTADCHandler> TMQTTADCHandler::GetADCHandler(const TMQTTADCHandler::TConfig& mqtt_config, const THandlerConfig& handler_config){
    if (handler_config.DeviceName == "ADCs") {
        std::shared_ptr<TMQTTADCHandlerMux> mqtt_handler(new TMQTTADCHandlerMux(mqtt_config, handler_config));
        return mqtt_handler;
    }
    else return nullptr;    
}

TMQTTADCHandler::TMQTTADCHandler(const TMQTTADCHandler::TConfig& mqtt_config, const THandlerConfig& handler_config)
    : TMQTTWrapper(mqtt_config),
      Config(handler_config)
{
}

TMQTTADCHandlerMux::TMQTTADCHandlerMux(const TMQTTADCHandler::TConfig& mqtt_config, const THandlerConfig& handler_config)
    : TMQTTADCHandler(mqtt_config, handler_config),
        ADC(GetSysfsPrefix(),
          handler_config.AveragingWindow,
          handler_config.MinSwitchIntervalMs,
          handler_config.Debug)

{
    for (int i = 0; i < 8; ++i)
        Channels.push_back(ADC.GetChannel("ADC" + std::to_string(i)));

	Connect();
}

void TMQTTADCHandlerMux::OnConnect(int rc)
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

void TMQTTADCHandlerMux::OnMessage(const struct mosquitto_message *)
{
    // NOOP
}

void TMQTTADCHandlerMux::OnSubscribe(int, int, const int *)
{
    if (Config.Debug)
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
        if (Config.Debug)
            std::cerr << "channel: " << channel.GetName() << " value: " << value << std::endl;
        Publish(NULL, GetChannelTopic(channel), to_string(value), 0, true);
    }
}
