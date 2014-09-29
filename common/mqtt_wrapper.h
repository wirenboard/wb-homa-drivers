#pragma once
#include <list>
#include <string>
#include <vector>
#include <memory>
#include <mosquittopp.h>

using namespace std;


class IMQTTObserver {
public:
    virtual ~IMQTTObserver();
    virtual void OnConnect(int rc)=0;
    virtual void OnMessage(const struct mosquitto_message *message)=0;
    virtual void OnSubscribe(int mid, int qos_count, const int *granted_qos)=0 ;
};

typedef std::shared_ptr<IMQTTObserver> PMQTTObserver;

class TMQTTClientBase
{
public:
    virtual ~TMQTTClientBase();
    virtual void Connect() = 0;
    virtual int Publish(int *mid, const string& topic, const string& payload="", int qos=0, bool retain=false) = 0;
    virtual int Subscribe(int *mid, const string& sub, int qos =0 ) = 0;
    virtual std::string Id() const = 0;
    void Observe(PMQTTObserver observer) { Observers.push_back(observer); }
    void Unobserve(PMQTTObserver observer) { Observers.remove(observer); }

protected:
    const std::list<PMQTTObserver>& GetObservers() const { return Observers; }

private:
    std::list<PMQTTObserver> Observers;
};

typedef std::shared_ptr<TMQTTClientBase> PMQTTClientBase;

class TMQTTClient : public mosqpp::mosquittopp, public TMQTTClientBase
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

    TMQTTClient(const TConfig& config);

    void on_connect(int rc);
    void on_message(const struct mosquitto_message *message);
    void on_subscribe(int mid, int qos_count, const int *granted_qos);

    inline void Connect() { connect(MQTTConfig.Host.c_str(), MQTTConfig.Port, MQTTConfig.Keepalive); };
    inline void ConnectAsync() { connect_async(MQTTConfig.Host.c_str(), MQTTConfig.Port, MQTTConfig.Keepalive); };

    int Publish(int *mid, const string& topic, const string& payload="", int qos=0, bool retain=false);

    // Loop for a given duration in milliseconds. The underlying select timeout
    // is set by 'timeout' parameter
    int LoopFor(int duration, int timeout = 60);

    inline int LoopForever(int timeout=-1, int max_packets=1) { return this->loop_forever(timeout, max_packets);};
    void StartLoop() { loop_start(); }
    void StopLoop() { disconnect(); loop_stop(); }

    //~ using mosqpp::mosquittopp::subscribe;
    int Subscribe(int *mid, const string& sub, int qos=0);

    string Id() const { return MQTTConfig.Id; }

protected:
    const TConfig& MQTTConfig;

private:
    std::list<PMQTTObserver> Observers;
};

typedef std::shared_ptr<TMQTTClient> PMQTTClient;

class TMQTTWrapper : public TMQTTClient, public IMQTTObserver,
                     public std::enable_shared_from_this<TMQTTWrapper> {
public:
    TMQTTWrapper(const TConfig& config);
};
