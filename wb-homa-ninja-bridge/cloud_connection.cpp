#include <iostream>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <memory>
#include <chrono>

#include "jsoncpp/json/json.h"
#include <mosquittopp.h>

#include "common/utils.h"
#include "common/mqtt_wrapper.h"
#include "cloud_connection.h"
#include "local_connection.h"
#include <cstdlib>
#include "jsoncpp/json/json.h"

using namespace std;

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::steady_clock;

TMQTTNinjaCloudHandler::TMQTTNinjaCloudHandler(const TMQTTNinjaCloudHandler::TConfig& mqtt_config, TMQTTNinjaBridgeHandler* parent)
    : TMQTTWrapper(mqtt_config)
    , BlockId(parent->BlockId)
    , CommandsReceiveTopic("$block/" + parent->BlockId + "/commands")
    , Token(parent->Token)
    , Parent(parent)

{
    username_pw_set("de4183317824e9daafed74439b9d1eef5ba1ccd4");
    tls_set("/etc/ssl/certs/ca-certificates.crt");
    tls_insecure_set(true);
    tls_opts_set(0);


	ConnectAsync();

    // spawn listener
    this->loop_start();

};

void TMQTTNinjaCloudHandler::OnConnect(int rc)
{
	printf("cloud connect with code %d.\n", rc);

    Parent->OnCloudConnect(rc);

    if (rc == 0) {
        Subscribe(NULL, CommandsReceiveTopic);


    }


}

void TMQTTNinjaCloudHandler::OnMessage(const struct mosquitto_message *message)
{
    string payload = static_cast<const char *>(message->payload);
	//~ cout << "Got msg: " << message->topic << " = " << payload << endl;

    string topic = message->topic;
    //~ const vector<string>& tokens = StringSplit(topic, '/');
    bool match;
    cout << CommandsReceiveTopic << endl;
    if (mosquitto_topic_matches_sub(CommandsReceiveTopic.c_str(), message->topic, &match) != MOSQ_ERR_SUCCESS) return;
    if (match) {
        //~ const string& device_id = tokens[2];

        Json::Value root;
        Json::Reader reader;

        if(!reader.parse(payload, root, false))  {
            cerr << "ERROR: Failed to parse command from Ninja Cloud" << endl
               << reader.getFormatedErrorMessages() << endl;
            return;
        }

        if (!root.isMember("DEVICE")) return;
        Json::Value & device_arr = root["DEVICE"];

        if (!device_arr.isArray()) return;
        Json::Value & device = device_arr[0];
        if (!device.isObject()) return;

        if (!device.isMember("GUID")) return;
        if (!device.isMember("DA")) return;

        const string & guid = device["GUID"].asString();
        const string & value = device["DA"].asString();

        //~ cout << "guid=" << guid << " , value="<<value << endl;
        auto res = ParseGUID(guid);
        if (res.Defined()) {
            Parent->OnCloudControlUpdate(res->first, res->second, value);
        }



    }


}
void TMQTTNinjaCloudHandler::OnSubscribe(int mid, int qos_count, const int *granted_qos)
{
}

string TMQTTNinjaCloudHandler::SelectorEscape(const string& str) const
{
    string escaped;
    char buf[3];

    escaped.reserve(str.size() * 3/2);

    for (const char& ch : str) {
        if (((ch >= 'a') && (ch <= 'z')) || ((ch >= 'A') && (ch <= 'Z')) || ((ch >= '0') && (ch <= '9')) || (ch == '_')) {
            escaped += ch;
        } else {
            snprintf(buf, 3, "%02x", ch);
            escaped += "-";
            escaped += buf;
        }

    }

    return escaped;
}

string TMQTTNinjaCloudHandler::SelectorUnescape(const string& str) const
{
    string unescaped;
    unescaped.reserve(str.size());
    char * p;

    size_t i = 0;
    while (i < str.size()) {
        if (str[i] == '-') {
            string substr = str.substr(i+1, 2);
            unsigned long ul = strtoul( substr.c_str(), & p, 16 );
            if ( * p == 0 ) {
                i += 3;
                unescaped += static_cast<unsigned char>(ul);
                continue;
            }
        }

        unescaped += str[i];
        ++i;
    }

    return unescaped;
}


string TMQTTNinjaCloudHandler::GetGUID(const string& device_id, const string& control_id) const
{
    string guid;

    return BlockId + "---" + SelectorEscape(device_id) + "---" + SelectorEscape(control_id);
}

// => device_id, control_id
TMaybe<pair<const string, const string>> TMQTTNinjaCloudHandler::ParseGUID(const string& guid) const
{
    const vector<string>& tokens = StringSplit(guid, "---");
    if (tokens.size() != 3) return NotDefinedMaybe;
    if (tokens[0] != BlockId) return NotDefinedMaybe;

    return pair<const string,const string>(SelectorUnescape(tokens[1]),SelectorUnescape(tokens[2]));
}

void TMQTTNinjaCloudHandler::SendDeviceData(const TControlDesc& control_desc)
{
    int vid;
    int did;

    // vid=0, did=2000 - sandbox device

    if (control_desc.Type == "switch") {
        vid = 0;
        did = 238;
    } else if (control_desc.Type == "temperature") {
        vid = 0;
        did = 9;
    } else {
        // unsupported device type
        return ;
    }


    const string name = control_desc.DeviceId + " " + control_desc.Id;
    SendDeviceData(GetGUID(control_desc.DeviceId, control_desc.Id),
                   vid, did, control_desc.Value, name);
}

void TMQTTNinjaCloudHandler::SendDeviceData(const string& guid, int v, int d, const string& da, const string& name)
{
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    static Json::FastWriter writer;

    Json::Value root(Json::objectValue);
    root["DEVICE"] = Json::Value(Json::arrayValue);
    root["DEVICE"].append(Json::Value(Json::objectValue));
    root["DEVICE"][0]["GUID"] = guid;
    root["DEVICE"][0]["V"] = v;
    root["DEVICE"][0]["D"] = d;
    root["DEVICE"][0]["DA"] = da;
    root["DEVICE"][0]["TIMESTAMP"] = to_string(millis);
    root["DEVICE"][0]["G"] = "none";

    if (!name.empty()) {
        root["DEVICE"][0]["name"] = name;
    }

    root["_token"] = Token;

    string payload = writer.write( root );

    Publish(NULL, string("") +"$cloud/" + BlockId + "/devices/all/data", payload);
}



