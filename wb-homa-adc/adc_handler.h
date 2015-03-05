#include<mosquittopp.h>
#include <fstream>
#include"common/utils.h"
#include"common/mqtt_wrapper.h"
#include"sysfs_adc.h"

using namespace std;
struct TChannel{
    int AveragingWindow = 10;
    int MinSwitchIntervalMs = 0;
    int PollInterval;
    string Type = "";
    vector<int> Gpios;
    vector<TMUXChannel> Mux;
};
    
struct THandlerConfig
{
    std::string DeviceName = "ADCs";
    bool Debug = false;
    vector<TChannel> Channels;
};

class TMQTTADCHandler : public TMQTTWrapper
{
public:
    TMQTTADCHandler(const TMQTTADCHandler::TConfig& mqtt_config, const THandlerConfig handler_config) ;

    void OnConnect(int rc) ;
    void OnMessage(const struct mosquitto_message *message);
    void OnSubscribe(int mid, int qos_count, const int *granted_qos);

    std::string GetChannelTopic(const TSysfsADCChannel& channel) const;
    void UpdateChannelValues();
    virtual void UpdateValue() ;
private:
    THandlerConfig Config;
    vector<std::shared_ptr<TSysfsADC>> ADCHandlers;

    vector<TSysfsADCChannel> Channels;
};

