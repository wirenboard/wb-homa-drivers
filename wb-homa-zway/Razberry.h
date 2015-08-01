#pragma once

#include <map>
#include <time.h>
#include "ZWaveBase.h"
#include <iostream>
#include <wbmqtt/mqtt_wrapper.h>
#include "jsoncpp/json/json.h"

namespace Json
{
	class Value;
};

using namespace std;

struct TZWayConfig 
{
    int Id = 0;
    int Port = 8083;
    string Host = "localhost";
    string Username = "";
    string Password = "";
    string DeviceName = "";
};
typedef pair<string, string> TPublishPair;

class TMQTTZWay;

class CRazberry : public ZWaveBase
{
public:
	CRazberry(TMQTTZWay* owner, const int ID, const std::string &ipaddress, const int port, const std::string &username, const std::string &password);
	~CRazberry(void);
    void SendCommand(const string& dev_id, const string& control_id, const string& payload);

	bool GetInitialDevices();
	bool GetUpdates();
private:
	const std::string GetControllerURL();
	const std::string GetRunURL(const std::string &cmd);
	void parseDevices(const Json::Value &devroot);

	_tZWaveDevice* FindDeviceByControl(const string& dev_id, const string& control_id);
    _tZWaveDevice* FindDeviceByScale(const int device_id, const int scale_id);
    _tZWaveDevice* FindDeviceByNodeId(const int node_id);
   
    void InsertControl(_tZWaveDevice device, const string& control_id);
	void SwitchOn(const _tZWaveDevice& device, const string& value);
	void SetThermostatSetPoint(const _tZWaveDevice& device, const string& value);
	bool IncludeDevice();
	bool ExcludeDevice(const int nodeID);
	bool RemoveFailedDevice(const int nodeID);
    void RunCMD(const std::string &cmd);
    void UpdateDevice(const std::string &path, const Json::Value &obj);
    string MqttEscape(const string& str);
    TMQTTZWay* Owner;
	std::string m_ipaddress;
	int m_port;
	std::string m_username;
	std::string m_password;
	int m_controllerID;
};

class TMQTTZWay: public TMQTTWrapper
{
    public :
        TMQTTZWay(const TMQTTZWay::TConfig& mqtt_config, TZWayConfig zway_config);
        ~TMQTTZWay();

        void OnConnect(int rc);
        void OnMessage(const struct mosquitto_message *message);
        void OnSubscribe(int mid, int qos_count, const int *granted_qos);
        void UpdateValues();
        void PublishDevice(const string& dev_id, const string& device_name);
        void PublishControlMeta(const string& dev_id, const string& control_id, const vector<TPublishPair>& meta);
        void PublishControl(const string& dev_id, const string& control_id, const string& value);
        void SubscribControl(const string& dev_id, const string& control_id);


    private :
        CRazberry razberry;
        string DeviceName;
};

