#pragma once

#include <iostream>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <getopt.h>
#include <memory>

#include "jsoncpp/json/json.h"
#include <mosquittopp.h>
#include <curl/curl.h>

#include <utils.h>
#include <mqtt_wrapper.h>
#include <http_helper.h>
using namespace std;

class TMQTTNinjaCloudHandler;
struct TControlDesc
{
    string Id;
    string DeviceId;
    string Type;
    string Value;
    bool Export = true;
};

class TMQTTNinjaBridgeHandler : public TMQTTWrapper
{
    friend class TMQTTNinjaCloudHandler;
    //~ typedef pair<TGpioDesc, TSysfsGpio> TChannelDesc;

	public:
        TMQTTNinjaBridgeHandler(const TMQTTNinjaBridgeHandler::TConfig& mqtt_config);
		~TMQTTNinjaBridgeHandler();

		void OnConnect(int rc);
		void OnMessage(const struct mosquitto_message *message);
		void OnSubscribe(int mid, int qos_count, const int *granted_qos);


		void OnCloudConnect(int rc);
        void SendDeviceDataToCloud(const TControlDesc& control_desc);


        //~ inline string GetChannelTopic(const TSysfsOnewireDevice& device);

        //~ void UpdateChannelValues();


        void OnCloudControlUpdate(const string& device_id, const string& control_id, const string& value);

        void WaitForActivation();
        void ConnectToCloud();

        //~ inline const string & GetBlockId() const {return BlockId; };
        //~ inline const string & GetToken() const {return Token; };

    private:
        inline string GetTokenFilename() const;
        bool TryActivateBlock();
        bool TryReadToken();
        void WriteToken();
        void ReadBlockId();

        int PublishStatus(const string& status);

        TControlDesc& GetOrAddControlDesc(const string& device_id, const string& control_id);

    private:
        //~ vector<TSysfsOnewireDevice> Channels;
        string BlockId;
        string Token;
        string VarDirectory;
        bool Activated;

        shared_ptr<TMQTTNinjaCloudHandler> NinjaCloudMqttHandler;

        map<pair<const string,const string>, TControlDesc> ControlsCache; //(device_id, control_id) => desc

};
