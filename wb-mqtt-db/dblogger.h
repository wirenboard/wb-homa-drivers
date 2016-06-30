#pragma once

#include <wbmqtt/utils.h>
#include <wbmqtt/mqtt_wrapper.h>
#include <wbmqtt/mqttrpc.h>
#include <chrono>
#include <string>
#include "jsoncpp/json/json.h"
#include  "SQLiteCpp/SQLiteCpp.h"

static const int WB_DB_VERSION = 2;
static const float RingBufferClearThreshold = 0.02; // ring buffer will be cleared on limit * (1 + RingBufferClearThreshold) entries

struct TLoggingChannel
{
    std::string Pattern;
};

struct TLoggingGroup
{
    std::vector<TLoggingChannel> Channels;
    int Values = 0;
    int ValuesTotal = 0;
    int MinInterval = 0;
    int MinUnchangedInterval = 0;
    std::string Id;
    int IntId;

};

struct TMQTTDBLoggerConfig
{
    std::vector<TLoggingGroup> Groups;
    std::string DBFile;
};

struct TChannel
{
    std::string Device;
    std::string Control;

    bool operator <(const TChannel& rhs) const {
        return std::tie(this->Device, this->Control) < std::tie(rhs.Device, rhs.Control);
    }
};


class TMQTTDBLogger: public TMQTTWrapper
{
public:
    TMQTTDBLogger(const TMQTTDBLogger::TConfig& mqtt_config, const TMQTTDBLoggerConfig config);
    ~TMQTTDBLogger();

    void OnConnect(int rc);
    void OnMessage(const struct mosquitto_message *message);
    void OnSubscribe(int mid, int qos_count, const int *granted_qos);

    void Init2();

    Json::Value GetValues(const Json::Value& input);
    Json::Value GetChannels(const Json::Value& input);

private:
    void InitDB();
    void CreateTables();
    int GetOrCreateChannelId(const TChannel& channel);
    int GetOrCreateDeviceId(const std::string& device);
    void InitChannelIds();
    void InitDeviceIds();
    void InitGroupIds();
    void InitCounterCaches();
    int ReadDBVersion();
    void UpdateDB(int prev_version);
    bool UpdateAccumulator(int channel_id, const string &payload);

    std::string Mask;
    std::unique_ptr<SQLite::Database> DB;
    TMQTTDBLoggerConfig LoggerConfig;
    std::shared_ptr<TMQTTRPCServer> RPCServer;
    std::map<TChannel, int> ChannelIds;
    std::map<std::string, int> DeviceIds;

    std::map<int, std::chrono::steady_clock::time_point> LastSavedTimestamps;
    std::map<int, int> ChannelRowNumberCache;
    std::map<int, int> GroupRowNumberCache;
    std::map<int, std::string> ChannelValueCache;

    // number of values, sum, min, max
    std::map<int, std::tuple<int, double, double, double>> ChannelAccumulator;

    const int DBVersion = WB_DB_VERSION;
};

