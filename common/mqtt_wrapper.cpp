#include <string>
#include <vector>
#include "utils.h"
#include <mosquittopp.h>
#include <iostream>
#include <chrono>


#include "mqtt_wrapper.h"

using namespace std;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::steady_clock;

IMQTTObserver::~IMQTTObserver() {}

TMQTTClientBase::~TMQTTClientBase() {}

TMQTTClient::TMQTTClient(const TConfig& config)
    : mosquittopp(config.Id.empty() ? NULL : config.Id.c_str())
    , MQTTConfig(config)
{
}

void TMQTTClient::on_connect(int rc)
{
    for (auto observer: GetObservers())
        observer->OnConnect(rc);
}

void TMQTTClient::on_message(const struct mosquitto_message *message)
{
    for (auto observer: GetObservers())
        observer->OnMessage(message);
}

void TMQTTClient::on_subscribe(int mid, int qos_count, const int *granted_qos)
{
    for (auto observer: GetObservers())
        observer->OnSubscribe(mid, qos_count, granted_qos);
}

int TMQTTClient::Publish(int *mid, const string& topic, const string& payload, int qos, bool retain) {
    return publish(mid, topic.c_str(), payload.size(), payload.c_str(), qos, retain);
}

int TMQTTClient::Subscribe(int *mid, const string& sub, int qos) {
    return subscribe(mid, sub.c_str(), qos);
}

int TMQTTClient::LoopFor(int duration, int timeout)
{
    steady_clock::time_point start = steady_clock::now();
    int cur_duration;
	int rc = 0;

    while (1) {
		rc = this->loop(timeout);

        cur_duration = duration_cast<milliseconds>(steady_clock::now() - start).count();
        if (cur_duration > duration) {
            return rc;
        }

		if(rc != 0) {
			this->reconnect();
        }
	}
}

TMQTTWrapper::TMQTTWrapper(const TConfig& config)
    : TMQTTClient(config)
{
    Observe(shared_from_this());
}
