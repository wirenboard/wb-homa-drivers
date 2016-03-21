#include "Razberry.h"
#include <wbmqtt/utils.h>
using namespace std;

TMQTTZWay::TMQTTZWay(const TMQTTZWay::TConfig& mqtt_config, TZWayConfig zway_config)
    : TMQTTWrapper(mqtt_config)
    , razberry(this, zway_config.Id, zway_config.Host, zway_config.Port, zway_config.Username, zway_config.Password)
    , DeviceName(zway_config.DeviceName)
{
    Connect();
}

TMQTTZWay::~TMQTTZWay()
{
}

void TMQTTZWay::OnConnect(int rc)
{
    cerr << "connected with code " << rc << endl;
    razberry.GetInitialDevices();

}

void TMQTTZWay::OnMessage(const struct mosquitto_message *message)
{
    string topic = message->topic;
    string payload = static_cast<const char *>(message->payload);
    cout << "Message is " << topic << endl;

    const vector<string>& tokens = StringSplit(topic, '/');

    if ((tokens.size() == 6) && (tokens[0] == "") && (tokens[1] == "devices") && (tokens[3] == "controls") && (tokens[5] == "on")) {
        string control_id = tokens[4];
        string dev_id = tokens[2];
        razberry.SendCommand(dev_id, control_id, payload);
        topic = "/" + tokens[1] +"/" + tokens[2] + "/" + tokens[3] + "/" + tokens[4];
        //Publish(NULL, topic, payload, 0 , true);
        cout << " topic is " << topic << endl;
        }
}


void TMQTTZWay::OnSubscribe(int mid, int qos_count, const int *granted_qos)
{
    cerr << "subscribed\n";
}

void TMQTTZWay::UpdateValues()
{
    razberry.GetUpdates();
}

void TMQTTZWay::PublishDevice(const string& dev_id, const string& device_name)
{
    string topic = string("/devices/") + dev_id;
    if (device_name != "") 
        Publish(NULL, topic + "/meta/name/", device_name, 0, true);
    else
        Publish(NULL, topic + "/meta/room/", "z-wave", 0, true);
}

void TMQTTZWay::PublishControlMeta(const string& dev_id, const string& control_id, const vector<TPublishPair>& meta)
{
    string topic = string("/devices/") + dev_id;
    topic += "/controls/" + control_id;
    for (const auto& meta_pair : meta) {
        Publish(NULL, topic + "/meta/" + meta_pair.first, meta_pair.second, 0, true);
    }
}

void TMQTTZWay::PublishControl(const string& dev_id, const string& control_id, const string& value)
{
    string topic = string("/devices/") + dev_id + "/controls/" + control_id;
    Publish(NULL, topic, value, 0, true);
}

void TMQTTZWay::SubscribControl(const string& dev_id, const string& control_id)
{
    string topic = string("/devices/") + dev_id + "/controls/" + control_id + "/on";
    Subscribe(NULL, topic); 
}
