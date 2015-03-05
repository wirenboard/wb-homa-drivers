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


TMQTTADCHandler::TMQTTADCHandler(const TMQTTADCHandler::TConfig& mqtt_config, THandlerConfig handler_config )
    : TMQTTWrapper(mqtt_config),
    Config(handler_config)
{
    for (auto& channel_config : Config.Channels){
        if (channel_config.Type == "mux"){
            std::shared_ptr<TSysfsADC> ADC(new TSysfsADC(GetSysfsPrefix(),channel_config.AveragingWindow, channel_config.MinSwitchIntervalMs, Config.Debug, channel_config.Gpios, channel_config.Mux));
            for (int i = 0; i < 8; ++i)
                Channels.push_back(ADC->GetChannel(i));
            ADCHandlers.push_back(ADC);
        }
    }
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
        //Publish(NULL, topic + "/meta/type", "text", 0, true);
        Publish(NULL, topic + "/meta/order", std::to_string(n++), 0, true);
        Publish(NULL, topic + "/meta/type", "voltage", 0, true);
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

string TMQTTADCHandler::GetChannelTopic(const TSysfsADCChannel& channel) const
{
    static string controls_prefix = std::string("/devices/") + MQTTConfig.Id + "/controls/";
    return controls_prefix + channel.GetName();
}

void TMQTTADCHandler::UpdateValue(){
    UpdateChannelValues();
}
void TMQTTADCHandler::UpdateChannelValues()
{
    for (auto channel : Channels) {
        int value = channel.GetValue();
        if (Config.Debug)
            std::cerr << "channel: " << channel.GetName() << " value: " << value << std::endl;
        float result = channel.GetMultiplier() * value;

        Publish(NULL, GetChannelTopic(channel), to_string(result), 0, true);
    }
}
