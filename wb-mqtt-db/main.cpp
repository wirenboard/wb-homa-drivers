#include <wbmqtt/utils.h>
#include <wbmqtt/mqtt_wrapper.h>
#include <wbmqtt/mqttrpc.h>
#include <chrono>
#include <iostream>
#include <string>
#include <ctime>
#include <unistd.h>
#include <fstream>

#include "jsoncpp/json/json.h"

#include  "SQLiteCpp/SQLiteCpp.h"

using namespace std;
using namespace std::chrono;



struct TLoggingChannel
{
    string Pattern;
};

struct TLoggingGroup
{
    vector<TLoggingChannel> Channels;
    int Values = 0;
    int ValuesTotal = 0;
    int MinInterval = 0;
    int MinUnchangedInterval = 0;
    string Id;

};

struct TMQTTDBLoggerConfig
{
    vector<TLoggingGroup> Groups;
    string DBFile;
};

struct TChannel
{
    string Device;
    string Control;

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

    private:
        void InitDB();
        string Mask;
        std::unique_ptr<SQLite::Database> DB;
        TMQTTDBLoggerConfig LoggerConfig;
        shared_ptr<TMQTTRPCServer> RPCServer;
        map<TChannel, steady_clock::time_point> LastSavedTimestamps;
        map<TChannel, int> ControlRowNumberCache;
        map<TChannel, int> GroupRowNumberCache;
        map<TChannel, string> ControlValueCache;

};

TMQTTDBLogger::TMQTTDBLogger (const TMQTTDBLogger::TConfig& mqtt_config, const TMQTTDBLoggerConfig config)
    : TMQTTWrapper(mqtt_config)
    , LoggerConfig(config)
{

    Connect();
    InitDB();
}

TMQTTDBLogger::~TMQTTDBLogger()
{
}

void TMQTTDBLogger::InitDB()
{
    DB.reset(new SQLite::Database(LoggerConfig.DBFile, SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE));

    DB->exec("CREATE TABLE IF NOT EXISTS data ( "
            "uid INTEGER PRIMARY KEY AUTOINCREMENT, "
            "device VARCHAR(255), "
            "control VARCHAR(255), "
            "value VARCHAR(255), "
            "timestamp DATETIME DEFAULT(STRFTIME('%Y-%m-%d %H:%M:%f', 'NOW')), "
            "group_id VARCHAR(32)"
            ")  "
            );

    DB->exec("CREATE INDEX IF NOT EXISTS data_topic ON data (device, control)");
    DB->exec("CREATE INDEX IF NOT EXISTS data_topic_timestamp ON data (device, control, timestamp)");

    DB->exec("CREATE INDEX IF NOT EXISTS data_gid ON data (group_id)");
    DB->exec("CREATE INDEX IF NOT EXISTS data_gid_timestamp ON data (group_id, timestamp)");



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
            TChannel channel = {tokens[2], tokens[4]};

            if ((group.MinInterval > 0) || (group.MinUnchangedInterval > 0)) {
                auto  last_saved = LastSavedTimestamps[channel];
                const auto& now = steady_clock::now();

                if (group.MinInterval > 0) {
                    if (duration_cast<milliseconds>(now - last_saved).count() < group.MinInterval * 1000) {
                        //limit rate, i.e. ignore this message
                        cout << "warning: rate limit for topic: " << topic <<  endl;
                        return;
                    }
                }


                if (group.MinUnchangedInterval > 0) {
                    if (ControlValueCache[channel] == payload) {
                        if (duration_cast<milliseconds>(now - last_saved).count() < group.MinUnchangedInterval * 1000) {
                            cout << "warning: rate limit (unchanged value) for topic: " << topic <<  endl;
                            return;
                        }
                    }

                    ControlValueCache[channel] = payload;
                }

                LastSavedTimestamps[channel] = now;
            }



            static SQLite::Statement insert_row_query(*DB, "INSERT INTO data (device, control, value, group_id) VALUES (?, ?, ?, ?)");

            insert_row_query.reset();
            insert_row_query.bind(1, channel.Device);
            insert_row_query.bind(2, channel.Control);
            insert_row_query.bind(3, payload);
            insert_row_query.bind(4, group.Id);

            insert_row_query.exec();
            cout << insert_row_query.getQuery() << endl;


            if (group.Values > 0) {

                static SQLite::Statement count_channel_query(*DB, "SELECT COUNT(*) FROM data WHERE device = ? and control = ?");
                count_channel_query.reset();
                count_channel_query.bind(1, channel.Device);
                count_channel_query.bind(2, channel.Control);
                if (!count_channel_query.executeStep()) {
                    throw TBaseException("cannot execute select count(*)");
                }

                cout << count_channel_query.getQuery() << endl;

                int found = count_channel_query.getColumn(0);

                if (found > group.Values) {
                    static SQLite::Statement clean_channel_query(*DB, "DELETE FROM data WHERE device = ? AND control = ? ORDER BY rowid ASC LIMIT ?");
                    clean_channel_query.reset();
                    clean_channel_query.bind(1, channel.Device);
                    clean_channel_query.bind(2, channel.Control);
                    clean_channel_query.bind(3, found - group.Values);

                    clean_channel_query.exec();
                    cout << clean_channel_query.getQuery() << endl;
                }
            }

            if (group.ValuesTotal > 0) {
                static SQLite::Statement count_group_query(*DB, "SELECT COUNT(*) FROM data WHERE group_id = ? ");
                count_group_query.reset();
                count_group_query.bind(1, group.Id);

                if (!count_group_query.executeStep()) {
                    throw TBaseException("cannot execute select count(*)");
                }

                int found = count_group_query.getColumn(0);

                if (found > group.ValuesTotal) {
                    static SQLite::Statement clean_group_query(*DB, "DELETE FROM data WHERE group_id = ? ORDER BY rowid ASC LIMIT ?");
                    clean_group_query.reset();
                    clean_group_query.bind(1, group.Id);
                    clean_group_query.bind(2, found - group.ValuesTotal);
                    clean_group_query.exec();
                    cout << clean_group_query.getQuery() << endl;

                }
            }

            break;
        }
    }

}

void TMQTTDBLogger::Init2()
{
    RPCServer = make_shared<TMQTTRPCServer>(shared_from_this(), "db_logger");
    RPCServer->RegisterMethod("history", "get_values", std::bind(&TMQTTDBLogger::GetValues, this, placeholders::_1));
    RPCServer->Init();
}

Json::Value TMQTTDBLogger::GetValues(const Json::Value& params)
{
    cout << "run method " << endl;

    Json::Value result;
    int limit = -1;
    double timestamp_gt = 0;
    int64_t uid_gt = -1;
    double timestamp_lt = 10675199167;




    if (params.isMember("timestamp")) {
        if (params["timestamp"].isMember("gt"))
            timestamp_gt = params["timestamp"]["gt"].asDouble();

        if (params["timestamp"].isMember("lt"))
            timestamp_gt = params["timestamp"]["lt"].asDouble();
    }

    if (params.isMember("uid")) {
        if (params["uid"].isMember("gt")) {
            uid_gt = params["uid"]["gt"].asInt64();
        }
    }



    if (params.isMember("limit"))
        limit = params["limit"].asInt();


    if (! params.isMember("channels"))
        throw TBaseException("no channels specified");


    int row_count = 0;
    bool has_more = false;


    for (const auto& channel_item : params["channels"]) {
        if (!(channel_item.isArray() && (channel_item.size() == 2)))
            throw TBaseException("'channels' items must be an arrays of size two ");

        const string& device_id = channel_item[0u].asString();
        const string& control_id = channel_item[1u].asString();


        static SQLite::Statement get_values_query(*DB, "SELECT uid, device, control, value,  (julianday(timestamp) - 2440587.5)*86400.0  FROM data "
                              "WHERE device=? AND control=? AND timestamp > datetime(?,'unixepoch') AND timestamp < datetime(?,'unixepoch') AND uid > ? ORDER BY uid ASC LIMIT ?");
        get_values_query.reset();
        get_values_query.bind(1, device_id);
        get_values_query.bind(2, control_id);
        get_values_query.bind(3, timestamp_gt);
        get_values_query.bind(4, timestamp_lt);
        get_values_query.bind(5, static_cast<sqlite3_int64>(uid_gt));
        get_values_query.bind(6, limit - row_count + 1); // we request one extra row to know whether there are more than 'limit' available

        while (get_values_query.executeStep()) {
            if (row_count >= limit) {
                has_more = true;
                break;
            }

            Json::Value row;
            row["uid"] = static_cast<int>(get_values_query.getColumn(0));
            row["device"] = get_values_query.getColumn(1).getText();
            row["control"] = get_values_query.getColumn(2).getText();
            row["value"] = get_values_query.getColumn(3).getText();
            row["timestamp"] = static_cast<double>(get_values_query.getColumn(4));
            result["values"].append(row);
            row_count += 1;
        }

        if (has_more) {
            // limit + 1 rows found, no need to request other channels
            break;
        }

    }

    if (has_more) {
        result["has_more"] = true;
    }


    return result;
}



int main (int argc, char *argv[])
{
    int rc;
    TMQTTDBLogger::TConfig mqtt_config;
    mqtt_config.Host = "localhost";
    mqtt_config.Port = 1883;
    string config_fname;
    int c;

    while ((c = getopt(argc, argv, "hp:H:c:")) != -1) {
        switch(c) {
            case 'p' :
                printf ("Option p with value '%s'\n", optarg);
                mqtt_config.Port = stoi(optarg);
                break;
            case 'H' :
                printf ("Option H with value '%s'\n", optarg);
                mqtt_config.Host = optarg;
                break;

            case 'c':
                printf ("option c with value '%s'\n", optarg);
                config_fname = optarg;
                break;

            case '?':
                printf ("?? Getopt returned character code 0%o ??\n",c);
            case 'h':
                printf ( "help menu\n");
            default:
                printf("Usage:\n wb-mqtt-timestamper [options] [mask]\n");
                printf("Options:\n");
                printf("\t-p PORT   \t\t\t set to what port wb-mqtt-timestamper should connect (default: 1883)\n");
                printf("\t-H IP     \t\t\t set to what IP wb-mqtt-timestamper should connect (default: localhost)\n");
                printf("\tmask      \t\t\t Mask, what topics subscribe to (default: /devices/+/controls/+)\n");
                return 0;
        }
    }


    if (config_fname.empty()) {
        cerr << "Please specify config file with -c option" << endl;
        return 1;
    }


    TMQTTDBLoggerConfig config;

    {
        // Let's parse config
        Json::Value root;
        Json::Reader reader;
        std::ifstream configStream(config_fname);
        bool parsedSuccess = reader.parse(configStream,
                                       root,
                                       false);

        if(not parsedSuccess)
        {
            cerr << "Failed to parse JSON" << endl
               << reader.getFormatedErrorMessages()
               << endl;
            return 1;
        }

        if (!root.isMember("database")) {
            throw TBaseException("database location should be specified in config");
        }

        config.DBFile = root["database"].asString();


        for(auto group_it = root["groups"].begin(); group_it !=root["groups"].end(); ++group_it) {
            const auto & group_item = *group_it;

            TLoggingGroup group;
            group.Id = group_it.key().asString();

            if (! group_item.isMember("channels")) {
                throw TBaseException("no channels specified for group");
            }

            for (const auto & channel_item : group_item["channels"]) {
                TLoggingChannel channel = {channel_item.asString()};
                group.Channels.push_back(channel);
            }

            if (group_item.isMember("values")) {
                if (group_item["values"].asInt() < 0)
                    throw TBaseException("'values' must be positive or zero");
                group.Values = group_item["values"].asInt();
            }

            if (group_item.isMember("values_total")) {
                if (group_item["values_total"].asInt() < 0)
                    throw TBaseException("'values_total' must be positive or zero");
                group.ValuesTotal = group_item["values_total"].asInt();
            }

            if (group_item.isMember("min_interval")) {
                if (group_item["min_interval"].asInt() < 0)
                    throw TBaseException("'min_interval' must be positive or zero");
                group.MinInterval = group_item["min_interval"].asInt();
            }

            if (group_item.isMember("min_unchanged_interval")) {
                if (group_item["min_unchanged_interval"].asInt() < 0)
                    throw TBaseException("'min_unchanged_interval' must be positive or zero");
                group.MinUnchangedInterval = group_item["min_unchanged_interval"].asInt();
            }



            config.Groups.push_back(group);
        }
    }




    mosqpp::lib_init();
    std::shared_ptr<TMQTTDBLogger> mqtt_db_logger(new TMQTTDBLogger(mqtt_config, config));
    mqtt_db_logger->Init();
    mqtt_db_logger->Init2();


    while(1) {
        rc = mqtt_db_logger->loop();
        if (rc != 0) {
            mqtt_db_logger->reconnect();
        }
    }
    return 0;
}


