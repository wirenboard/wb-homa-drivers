#include <iostream>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <sstream>
#include <iomanip>

#include "adc_handler.h"

using namespace std;

namespace {
    std::string GetSysfsPrefix()
    {
        const char* prefix = getenv("WB_SYSFS_PREFIX");
        return prefix ? prefix : "/sys";
    }
}


TMQTTAdcHandler::TMQTTAdcHandler(const TMQTTAdcHandler::TConfig& mqtt_config, THandlerConfig handler_config)
    : TMQTTWrapper(mqtt_config),
    Config(handler_config)
{
    for (auto& channel_config : Config.Channels){
        if (channel_config.Type == "mux") {
            AdcHandlers.emplace_back(new TSysfsAdcMux(GetSysfsPrefix(), Config.Debug, channel_config));
		} else {
            AdcHandlers.emplace_back(new TSysfsAdcPhys(GetSysfsPrefix(), Config.Debug, channel_config));
        }
        for (int i = 0; i < AdcHandlers.back()->GetNumberOfChannels(); ++i) {
			Channels.push_back(AdcHandlers.back()->GetChannel(i));
		}

    }
	Connect();
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
        std::string topic = GetChannelTopic(*channel);
        std::string type = channel->GetType();
        Publish(NULL, topic + "/meta/order", std::to_string(n++), 0, true);
        Publish(NULL, topic + "/meta/type", type, 0, true);
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

void TMQTTAdcHandler::UpdateValue()
{
    UpdateChannelValues();
}
void TMQTTAdcHandler::UpdateChannelValues()
{
    for (auto channel : Channels) {
        float value = channel->GetValue();
        if (Config.Debug)
            std::cerr << "channel: " << channel->GetName() << " value: " << value << std::endl;

		std::ostringstream out;
		out << std::fixed << std::setprecision(channel->DecimalPlaces) << value;
        Publish(NULL, GetChannelTopic(*channel), out.str(), 0, true);
    }
}
