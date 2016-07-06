#include "dblogger.h"

#include <tuple>
#include <glog/logging.h>

using namespace std;
using namespace std::chrono;

bool TMQTTDBLogger::UpdateAccumulator(int channel_id, const string &payload)
{
    double value = 0.0;

    // try to cast value to double and run stats
    try {
        value = stod(payload);
    } catch (...) {
        return false; // no processing for uncastable values
    }
    
    ChannelDataCache[channel_id].Accumulated = true;
    auto& accum = ChannelDataCache[channel_id].Accumulator;

    accum.ValueCount++;

    if (accum.ValueCount == 1) {
        accum.Min = accum.Max = accum.Sum = value;
    } else {
        if (accum.Min > value)
            accum.Min = value;
        if (accum.Max < value)
            accum.Max = value;
        accum.Sum += value;
    }
    
    return true;
}

tuple<int, int> TMQTTDBLogger::GetOrCreateIds(const TChannelName &name)
{
    return GetOrCreateIds(name.Device, name.Control);
}

tuple<int, int> TMQTTDBLogger::GetOrCreateIds(const string &device, const string &control)
{
    int channel_int_id = GetOrCreateChannelId({ device, control });
    int device_int_id = GetOrCreateDeviceId(device);

    return tie(channel_int_id, device_int_id);
}

tuple<int, int> TMQTTDBLogger::GetOrCreateIds(const string &topic)
{
    const vector<string>& tokens = StringSplit(topic, '/');
    return GetOrCreateIds(tokens[2], tokens[4]);
}

void TMQTTDBLogger::OnConnect(int rc)
{
    for (const auto& group : LoggerConfig.Groups) {
        for (const auto& channel : group.Channels) {
            Subscribe(NULL, channel.Pattern);
        }
    }
}

void TMQTTDBLogger::OnSubscribe(int mid, int qos_count, const int *granted_qos)
{
}

void TMQTTDBLogger::OnMessage(const struct mosquitto_message *message)
{
    if (!message->payload)
        return;

    string topic = message->topic;
    string payload = static_cast<const char*>(message->payload);


    bool match;
    int channel_int_id;

    for (auto& group : LoggerConfig.Groups) {
        match = false;
        for (auto& channel : group.Channels) {
            if (TopicMatchesSub(channel.Pattern, message->topic)) {
                match = true;
                break;
            }
        }

        if (match) {
            VLOG(2) << "MQTT message from topic " << topic << ": \"" << payload 
                    << "\", parsed as group \"" << group.Id << "\""; 

            tie(channel_int_id, std::ignore) = GetOrCreateIds(topic);
            auto& channel_data = ChannelDataCache[channel_int_id];

            DVLOG(3) << "Channel ID for topic " << topic << " is " << channel_int_id;

            if (payload != channel_data.LastValue) {
                channel_data.Changed = true;
            }

            // FIXME: need to fix it when global data types definitions will be ready
            UpdateAccumulator(channel_int_id, payload);
            channel_data.LastValue = payload;
            channel_data.LastProcessed = steady_clock::now();
            channel_data.Retained = message->retain;

            group.ChannelIds.insert(channel_int_id);

            break;
        }
    }

}
