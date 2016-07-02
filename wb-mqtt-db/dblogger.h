#pragma once

#include <wbmqtt/utils.h>
#include <wbmqtt/mqtt_wrapper.h>
#include <wbmqtt/mqttrpc.h>
#include <chrono>
#include <string>
#include <set>
#include "jsoncpp/json/json.h"
#include  "SQLiteCpp/SQLiteCpp.h"

static const int    WB_DB_VERSION = 2;
static const float  RingBufferClearThreshold = 0.02; // ring buffer will be cleared on limit * (1 + RingBufferClearThreshold) entries
static const int    WB_DB_LOOP_TIMEOUT = 10; // loop will be interrupted at least once in this interval (in ms) for
                                             // DB update event

struct TLoggingChannel
{
    std::string Pattern;
};

struct TChannelName
{
    std::string Device;
    std::string Control;

    bool operator<(const TChannelName& rhs) const {
        return std::tie(this->Device, this->Control) < std::tie(rhs.Device, rhs.Control);
    }

};

struct TChannel
{
    TChannelName Name;

    std::string LastValue;
    // number of values, sum, min, max
    std::tuple<int, double, double, double> Accumulator;

    std::chrono::steady_clock::time_point LastProcessed;

    int RowCount;

    bool Changed;
    bool Accumulated;
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
    
    // internal fields - for timer processing
    std::set<int> ChannelIds;
    
    std::chrono::steady_clock::time_point LastSaved;
    std::chrono::steady_clock::time_point LastUSaved;
};

typedef std::shared_ptr<TLoggingGroup> PLoggingGroup;

struct TMQTTDBLoggerConfig
{
    std::vector<TLoggingGroup> Groups;
    std::string DBFile;
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

    std::chrono::steady_clock::time_point ProcessTimer(std::chrono::steady_clock::time_point next_call);

private:
    void InitDB();
    void CreateTables();
    int GetOrCreateChannelId(const TChannelName& channel);
    int GetOrCreateDeviceId(const std::string& device);
    void InitChannelIds();
    void InitDeviceIds();
    void InitGroupIds();
    void InitCaches();
    int ReadDBVersion();
    void UpdateDB(int prev_version);
    bool UpdateAccumulator(int channel_id, const string &payload);
    void WriteChannel(TChannel &ch, TLoggingGroup &group);
    
    std::tuple<int, int> GetOrCreateIds(const std::string &device, const std::string &control);
    std::tuple<int, int> GetOrCreateIds(const std::string &topic);
    std::tuple<int, int> GetOrCreateIds(const TChannelName &name);

    std::string Mask;
    std::unique_ptr<SQLite::Database> DB;
    TMQTTDBLoggerConfig LoggerConfig;
    std::shared_ptr<TMQTTRPCServer> RPCServer;
    std::map<TChannelName, int> ChannelIds;
    std::map<std::string, int> DeviceIds;

    // std::map<int, std::chrono::steady_clock::time_point> LastSavedTimestamps;
    // std::map<int, int> ChannelRowNumberCache;
    // std::map<int, std::string> ChannelValueCache;
    std::map<int, TChannel> ChannelDataCache;

    std::map<int, int> GroupRowNumberCache;

    // number of values, sum, min, max
    // std::map<int, std::tuple<int, double, double, double>> ChannelAccumulator;

    const int DBVersion = WB_DB_VERSION;
};

