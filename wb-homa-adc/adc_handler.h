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
    vector<int> Gpios;
};


class TMQTTADCHandler : public TMQTTWrapper
{
public:
    TMQTTADCHandler(const TMQTTADCHandler::TConfig& mqtt_config, const THandlerConfig& handler_config) ;

    virtual void OnConnect(int rc) = 0;
    virtual void OnMessage(const struct mosquitto_message *message) = 0;
    virtual void OnSubscribe(int mid, int qos_count, const int *granted_qos) = 0;
    static std::shared_ptr<TMQTTADCHandler> GetADCHandler(const TMQTTADCHandler::TConfig& mqtt_config, const THandlerConfig& handler_config);

    //std::string GetChannelTopic(const TSysfsADCChannel& channel) const;
    //void UpdateChannelValues();
    virtual void UpdateValue() = 0;
private:
    THandlerConfig Config;
    TSysfsADC ADC;
    //vector<TSysfsADCChannel> Channels;
};

class TMQTTADCHandlerMux : public TMQTTADCHandler
{
public:
    TMQTTADCHandlerMux(const TMQTTADCHandlerMux::TConfig& mqtt_config, const THandlerConfig& handler_config);

    void OnConnect(int rc);
    void OnMessage(const struct mosquitto_message *message);
    void OnSubscribe(int mid, int qos_count, const int *granted_qos);

    std::string GetChannelTopic(const TSysfsADCChannel& channel) const;
    void UpdateChannelValues();
    void UpdateValue();
private:
    THandlerConfig Config;
    TSysfsADC ADC;
    vector<TSysfsADCChannel> Channels;
};


