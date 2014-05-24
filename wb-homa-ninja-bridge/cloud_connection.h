#pragma once
#include <memory>
#include <common/utils.h>

using namespace std;
class TMQTTNinjaBridgeHandler;
class TControlDesc;


class TMQTTNinjaCloudHandler : public TMQTTWrapper
{
	public:
        TMQTTNinjaCloudHandler(const TMQTTNinjaCloudHandler::TConfig& mqtt_config, TMQTTNinjaBridgeHandler* parent);
		//~ ~TMQTTNinjaCloudHandler();

		void OnConnect(int rc);
		void OnMessage(const struct mosquitto_message *message);
		void OnSubscribe(int mid, int qos_count, const int *granted_qos);

        //~ void on_log(int level, const char *str) { cerr << "on log: level=" << level << " " << str << endl; };

        void SendDeviceData(const string& guid, int v, int d, const string& da, const string& name);
        void SendDeviceData(const TControlDesc& control_desc);

		//~ void on_error() {return;};

    private:
        string SelectorEscape(const string& str) const;
        string SelectorUnescape(const string& str) const;
        string GetGUID(const string& device_id, const string& control_id) const;

        TMaybe<pair<const string, const string>> ParseGUID(const string& guid) const; // => maybe(device_id, control_id)


    private:
        const string & BlockId;
        const string CommandsReceiveTopic;
        const string & Token;

        TMQTTNinjaBridgeHandler * Parent;


};
