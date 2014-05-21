#pragma once
#include <string>
#include <vector>
#include <mosquittopp.h>

using namespace std;



class TMQTTWrapper : public mosqpp::mosquittopp
{
	public:

        struct TConfig
        {
                int Port;
                string Host;
                int Keepalive;
                string Id;

                TConfig()
                    : Port(1883)
                    , Host("localhost")
                    , Keepalive(60)
                    , Id("")
                {}
        };

        TMQTTWrapper(const TConfig& config);

		inline void on_connect(int rc) { this->OnConnect(rc); };
		inline void on_message(const struct mosquitto_message *message) { this->OnMessage(message); };
		inline void on_subscribe(int mid, int qos_count, const int *granted_qos) { this->OnSubscribe(mid, qos_count, granted_qos); };

        inline void Connect() {connect(MQTTConfig.Host.c_str(), MQTTConfig.Port, MQTTConfig.Keepalive); };
		int Publish(int *mid, const string& topic, const string& payload="", int qos=0, bool retain=false);

        //~ using mosqpp::mosquittopp::subscribe;
		int Subscribe(int *mid, const string& sub, int qos=0);

        // override
		virtual void OnConnect(int rc)=0;
		virtual void OnMessage(const struct mosquitto_message *message)=0;
		virtual void OnSubscribe(int mid, int qos_count, const int *granted_qos)=0 ;

    protected:
        const TConfig& MQTTConfig;
};

