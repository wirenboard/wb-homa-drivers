#include "dblogger.h"

using namespace std;
using namespace std::chrono;

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

    // retained messages received on startup are ignored
    if (message->retain)
        return;

    high_resolution_clock::time_point t1 = high_resolution_clock::now(); //FIXME: debug


    string topic = message->topic;
    string payload = static_cast<const char*>(message->payload);


    bool match;

    for (const auto& group : LoggerConfig.Groups) {
        match = false;
        for (const auto& channel : group.Channels) {
            if (TopicMatchesSub(channel.Pattern, message->topic)) {
                match = true;
                break;
            }
        }

        if (match) {
            const vector<string>& tokens = StringSplit(topic, '/');
            int channel_int_id = GetOrCreateChannelId({tokens[2], tokens[4]});
            int device_int_id = GetOrCreateDeviceId(tokens[2]);

            // FIXME: need to fix it when global data types definitions will be ready
            bool accumulated = UpdateAccumulator(channel_int_id, payload);

            if ((group.MinInterval > 0) || (group.MinUnchangedInterval > 0)) {
                auto  last_saved = LastSavedTimestamps[channel_int_id];
                const auto& now = steady_clock::now();

                if (group.MinInterval > 0) {
                    if (duration_cast<milliseconds>(now - last_saved).count() < group.MinInterval * 1000) {
                        //limit rate, i.e. ignore this message
                        cout << "warning: rate limit for topic: " << topic <<  endl;
                        return;
                    }
                }


                if (group.MinUnchangedInterval > 0) {
                    if (ChannelValueCache[channel_int_id] == payload) {
                        if (duration_cast<milliseconds>(now - last_saved).count() < group.MinUnchangedInterval * 1000) {
                            cout << "warning: rate limit (unchanged value) for topic: " << topic <<  endl;
                            return;
                        }
                    }
                    
                    ChannelValueCache[channel_int_id] = payload;
                }

                LastSavedTimestamps[channel_int_id] = now;
            }


            static SQLite::Statement insert_row_query(*DB, "INSERT INTO data (device, channel, value, group_id, min, max) VALUES (?, ?, ?, ?, ?, ?)");

            insert_row_query.reset();
            insert_row_query.bind(1, device_int_id);
            insert_row_query.bind(2, channel_int_id);
            insert_row_query.bind(4, group.IntId);
            
            // min, max and average
            if (accumulated) {
                auto& accum = ChannelAccumulator[channel_int_id];
                int& num_values = get<0>(accum);
                double& sum = get<1>(accum);
                double& min = get<2>(accum);
                double& max = get<3>(accum);

                insert_row_query.bind(3, num_values > 0 ? sum / num_values : 0.0); // avg

                insert_row_query.bind(5, min); // min
                insert_row_query.bind(6, max); // max

                num_values = 0;
            } else {
                insert_row_query.bind(3, payload); // avg == value
            }


            insert_row_query.exec();
            cout << insert_row_query.getQuery() << endl;


            // local cache is needed here since SELECT COUNT are extremely slow in sqlite
            // so we only ask DB at startup. This applies to two if blocks below.

            if (group.Values > 0) {
                if ((++ChannelRowNumberCache[channel_int_id]) > group.Values * (1 + RingBufferClearThreshold) ) {
                    static SQLite::Statement clean_channel_query(*DB, "DELETE FROM data WHERE channel = ? ORDER BY rowid ASC LIMIT ?");
                    clean_channel_query.reset();
                    clean_channel_query.bind(1, channel_int_id);
                    clean_channel_query.bind(2, ChannelRowNumberCache[channel_int_id] - group.Values);

                    clean_channel_query.exec();
                    cout << clean_channel_query.getQuery() << endl;
                    ChannelRowNumberCache[channel_int_id] = group.Values;
                }
            }

            if (group.ValuesTotal > 0) {
                if ((++GroupRowNumberCache[group.IntId]) > group.ValuesTotal * (1 + RingBufferClearThreshold)) {
                    static SQLite::Statement clean_group_query(*DB, "DELETE FROM data WHERE group_id = ? ORDER BY rowid ASC LIMIT ?");
                    clean_group_query.reset();
                    clean_group_query.bind(1, group.IntId);
                    clean_group_query.bind(2, GroupRowNumberCache[group.IntId] - group.ValuesTotal);
                    clean_group_query.exec();
                    cout << clean_group_query.getQuery() << endl;
                    GroupRowNumberCache[group.IntId] = group.ValuesTotal;
                }
            }

            break;
        }
    }

    //FIXME: debug
    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();

    cout << "msg for " << topic << " took " << duration << "ms" << endl;
}

