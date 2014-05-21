#include <string>
#include <vector>
#include "utils.h"
#include <mosquittopp.h>

#include "mqtt_wrapper.h"

using namespace std;

int TMQTTWrapper::Publish(int *mid, const string& topic, const string& payload, int qos, bool retain) {
    return publish(mid, topic.c_str(), payload.size(), payload.c_str(), qos, retain);
}

int TMQTTWrapper::Subscribe(int *mid, const string& sub, int qos) {
    return subscribe(mid, sub.c_str(), qos);
}

TMQTTWrapper::TMQTTWrapper(const TConfig& config)
    : mosquittopp(config.Id.empty() ? NULL : config.Id.c_str())
    , MQTTConfig(config)
{
};




