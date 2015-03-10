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


TMQTTAdcHandler::TMQTTAdcHandler(const TMQTTAdcHandler::TConfig& mqtt_config, THandlerConfig handler_config )
    : TMQTTWrapper(mqtt_config),
    Config(handler_config)
{
    for (auto& channel_config : Config.Channels){
        std::shared_ptr<TSysfsAdc> adc_handler(nullptr); 
        if (channel_config.Type == "mux"){
            adc_handler.reset(new TSysfsAdcMux(GetSysfsPrefix(), Config.Debug, channel_config));
                    }else {
            adc_handler.reset(new TSysfsAdcPhys(GetSysfsPrefix(), Config.Debug, channel_config));
        }
        for (int i = 0; i < adc_handler->GetNumberOfChannels(); ++i)
                Channels.push_back(adc_handler->GetChannel(i));
        AdcHandlers.push_back(adc_handler);

    }
	Connect();
}

TMQTTAdcHandler::~TMQTTAdcHandler(){
    AdcHandlers.clear();
    Channels.clear();
}

void TMQTTAdcHandler::OnConnect(int rc)
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
        //Publish(NULL, topic + "/meta/type", "text", 0, true);
        Publish(NULL, topic + "/meta/order", std::to_string(n++), 0, true);
        Publish(NULL, topic + "/meta/type", "voltage", 0, true);
    }
}

void TMQTTAdcHandler::OnMessage(const struct mosquitto_message *)
{
    // NOOP
}

void TMQTTAdcHandler::OnSubscribe(int, int, const int *)
{
    if (Config.Debug)
        std::cerr << "Subscription succeeded." << std::endl;
}

string TMQTTAdcHandler::GetChannelTopic(const TSysfsAdcChannel& channel) const
{
    static string controls_prefix = std::string("/devices/") + MQTTConfig.Id + "/controls/";
    return controls_prefix + channel.GetName();
}

void TMQTTAdcHandler::UpdateValue(){
    UpdateChannelValues();
}
void TMQTTAdcHandler::UpdateChannelValues()
{
    for (auto channel : Channels) {
        int value = channel.GetValue();
        if (Config.Debug)
            std::cerr << "channel: " << channel.GetName() << " value: " << value << std::endl;
        float result = channel.GetMultiplier() * value;

        Publish(NULL, GetChannelTopic(channel), to_string(result), 0, true);
    }
}
