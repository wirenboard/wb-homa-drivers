#include<mosquittopp.h>
#include <fstream>
#include"common/utils.h"
#include"common/mqtt_wrapper.h"
#include"sysfs_adc.h"
struct THandlerConfig
{
    std::string DeviceName = "ADCs";
    bool Debug = false;
    int AveragingWindow = 10;
    int MinSwitchIntervalMs = 0;
    string Id;
    int Multiplier;
    int Gpio;
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

