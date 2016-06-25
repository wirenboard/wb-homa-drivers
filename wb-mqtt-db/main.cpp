#include <wbmqtt/utils.h>
#include <wbmqtt/mqtt_wrapper.h>
#include <wbmqtt/mqttrpc.h>
#include <chrono>
#include <iostream>
#include <string>
#include <ctime>
#include <unistd.h>
#include <fstream>
#include <signal.h>

#include "jsoncpp/json/json.h"

#include  "SQLiteCpp/SQLiteCpp.h"


sig_atomic_t running = 1;

/* it handles only SIGTERM and SIGINT to exit gracefully */
void sig_handler(int signal)
{
    running = 0;
}

using namespace std;
using namespace std::chrono;

const float RingBufferClearThreshold = 0.02; // ring buffer will be cleared on limit * (1 + RingBufferClearThreshold) entries
        
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
    int IntId;

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
        Json::Value GetChannels(const Json::Value& input);

    private:
        void InitDB();
        void CreateTables();
        int GetOrCreateChannelId(const TChannel& channel);
        int GetOrCreateDeviceId(const string& device);
        void InitChannelIds();
        void InitDeviceIds();
        void InitGroupIds();
        void InitCounterCaches();
        int ReadDBVersion();
        void UpdateDB(int prev_version);
        bool UpdateAccumulator(int channel_id, const string &payload);

        string Mask;
        std::unique_ptr<SQLite::Database> DB;
        TMQTTDBLoggerConfig LoggerConfig;
        shared_ptr<TMQTTRPCServer> RPCServer;
        map<TChannel, int> ChannelIds;
        map<string, int> DeviceIds;

        map<int, steady_clock::time_point> LastSavedTimestamps;
        map<int, int> ChannelRowNumberCache;
        map<int, int> GroupRowNumberCache;
        map<int, string> ChannelValueCache;

        // number of values, sum, min, max
        map<int, tuple<int, double, double, double>> ChannelAccumulator;

        const int DBVersion = 2;

};

TMQTTDBLogger::TMQTTDBLogger (const TMQTTDBLogger::TConfig& mqtt_config, const TMQTTDBLoggerConfig config)
    : TMQTTWrapper(mqtt_config)
    , LoggerConfig(config)
{

    InitDB();
    Connect();
}

TMQTTDBLogger::~TMQTTDBLogger()
{
}


void TMQTTDBLogger::CreateTables()
{
    DB->exec("CREATE TABLE IF NOT EXISTS devices ( "
             "int_id INTEGER PRIMARY KEY AUTOINCREMENT, "
             "device VARCHAR(255) UNIQUE "
             " )  ");

    DB->exec("CREATE TABLE IF NOT EXISTS channels ( "
             "int_id INTEGER PRIMARY KEY AUTOINCREMENT, "
             "device VARCHAR(255), "
             "control VARCHAR(255) "
             ")  ");

    DB->exec("CREATE TABLE IF NOT EXISTS groups ( "
             "int_id INTEGER PRIMARY KEY AUTOINCREMENT, "
             "group_id VARCHAR(255) "
             ")  ");


    DB->exec("CREATE TABLE IF NOT EXISTS data ("
			 "uid INTEGER PRIMARY KEY AUTOINCREMENT, "
			 "device INTEGER,"
			 "channel INTEGER,"
			 "value VARCHAR(255),"
			 "timestamp REAL DEFAULT(julianday('now')),"
			 "group_id INTEGER,"
             "max VARCHAR(255),"
             "min VARCHAR(255)"
			 ")"
			);

    DB->exec("CREATE TABLE IF NOT EXISTS variables ("
			 "name VARCHAR(255) PRIMARY KEY, "
			 "value VARCHAR(255) )"
			);


    DB->exec("CREATE INDEX IF NOT EXISTS data_topic ON data (channel)");

    // NOTE: the following index is a "low quality" one according to sqlite documentation. However, reversing the order of columns results in factor of two decrease in SELECT performance. So we leave it here as it is. 
    DB->exec("CREATE INDEX IF NOT EXISTS data_topic_timestamp ON data (channel, timestamp)");

    DB->exec("CREATE INDEX IF NOT EXISTS data_gid ON data (group_id)");
    DB->exec("CREATE INDEX IF NOT EXISTS data_gid_timestamp ON data (group_id, timestamp)");

	{
		SQLite::Statement query(*DB, "INSERT OR REPLACE INTO variables (name, value) VALUES ('db_version', ?)");
		query.bind(1, DBVersion);
		query.exec();
	}

}


void TMQTTDBLogger::InitCounterCaches()
{
    SQLite::Statement count_group_query(*DB, "SELECT COUNT(*) as cnt, group_id FROM data GROUP BY group_id ");
	while (count_group_query.executeStep()) {
        GroupRowNumberCache[count_group_query.getColumn(1)] = count_group_query.getColumn(0);
    }

    SQLite::Statement count_channel_query(*DB, "SELECT COUNT(*) as cnt, channel FROM data GROUP BY channel ");
    while (count_channel_query.executeStep()) {
        ChannelRowNumberCache[count_channel_query.getColumn(1)] = count_channel_query.getColumn(0);
    }

    SQLite::Statement last_ts_query(*DB, "SELECT (MAX(timestamp) - 2440587.5) * 86400.0 AS ts, channel FROM data GROUP BY channel ");
    while (last_ts_query.executeStep()) {
        auto d = milliseconds(static_cast<long long>(last_ts_query.getColumn(0)) * 1000);
        auto current_tp = steady_clock::time_point(d);
        LastSavedTimestamps[last_ts_query.getColumn(1)] = current_tp;
    }
}


void TMQTTDBLogger::InitChannelIds()
{
	SQLite::Statement query(*DB, "SELECT int_id, device, control FROM channels");
    while (query.executeStep()) {
        ChannelIds[{query.getColumn(1).getText(),
					query.getColumn(2).getText()}] = query.getColumn(0);
    }
}

void TMQTTDBLogger::InitDeviceIds()
{
	SQLite::Statement query(*DB, "SELECT int_id, device FROM devices");
    while (query.executeStep()) {
        DeviceIds[query.getColumn(1).getText()] = query.getColumn(0);
    }
}

void TMQTTDBLogger::InitGroupIds()
{
	map<string, int> stored_group_ids;
	{
		SQLite::Statement query(*DB, "SELECT int_id, group_id FROM groups");
	    while (query.executeStep()) {
			stored_group_ids[query.getColumn(1).getText()] = query.getColumn(0);
		}
	}

    for (auto& group : LoggerConfig.Groups) {
		auto it = stored_group_ids.find(group.Id);
		if (it != stored_group_ids.end()) {
			group.IntId = it->second;
		} else {
			// new group, no id is stored

			static SQLite::Statement query(*DB, "INSERT INTO groups (group_id) VALUES (?) ");
			query.reset();
			query.bind(1, group.Id);
			query.exec();
			group.IntId = DB->getLastInsertRowid();
		}
	}
}

int TMQTTDBLogger::ReadDBVersion()
{
	if (!DB->tableExists("variables")) {
		return 0;
	}

	SQLite::Statement query(*DB, "SELECT value FROM variables WHERE name = 'db_version'");
    while (query.executeStep()) {
		return query.getColumn(0).getInt();
	}

	return 0;
}

void TMQTTDBLogger::UpdateDB(int prev_version)
{
	SQLite::Transaction transaction(*DB);

    switch (prev_version) {
    case 0:
	    // Begin transaction

	    DB->exec("ALTER TABLE data RENAME TO tmp");

		// drop existing indexes
		DB->exec("DROP INDEX data_topic");
		DB->exec("DROP INDEX data_topic_timestamp");
		DB->exec("DROP INDEX data_gid");
		DB->exec("DROP INDEX data_gid_timestamp");

		// create tables with most recent schema
		CreateTables();

		// generate internal integer ids from old data table
	    DB->exec("INSERT OR IGNORE INTO devices (device) SELECT device FROM tmp GROUP BY device");
	    DB->exec("INSERT OR IGNORE INTO channels (device, control) SELECT device, control FROM tmp GROUP BY device, control");
	    DB->exec("INSERT OR IGNORE INTO groups (group_id) SELECT group_id FROM tmp GROUP BY group_id");

		// populate data table using values from old data table
		DB->exec("INSERT INTO data(uid, device, channel,value,timestamp,group_id) "
                  "SELECT uid, devices.int_id, channels.int_id, value, julianday(timestamp), groups.int_id FROM tmp "
                  "LEFT JOIN devices ON tmp.device = devices.device "
                  "LEFT JOIN channels ON tmp.device = channels.device AND tmp.control = channels.control "
                  "LEFT JOIN groups ON tmp.group_id = groups.group_id ");

	    DB->exec("DROP TABLE tmp");

		transaction.commit();

		// defragment database
		DB->exec("VACUUM");

    case 1:
        // In versions >= 2, there is a difference in 'data' table:
        // add data.max, data.min columns

        DB->exec("ALTER TABLE data ADD COLUMN max VARCHAR(255)");
        DB->exec("ALTER TABLE data ADD COLUMN min VARCHAR(255)");

        DB->exec("UPDATE data SET max = value");
        DB->exec("UPDATE data SET min = value");

        transaction.commit();

        DB->exec("VACUUM");
        break;

    default:
		throw TBaseException("Unsupported DB version. Please consider deleting DB file.");
	}
}

void TMQTTDBLogger::InitDB()
{
	DB.reset(new SQLite::Database(LoggerConfig.DBFile, SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE));

	if (!DB->tableExists("data")) {
		// new DB file created
		std::cerr << "Creating tables" << std::endl;
		CreateTables();
	} else {
		int file_db_version = ReadDBVersion();
		if (file_db_version > DBVersion) {
			throw TBaseException("Database file is created by newer version of wb-mqtt-db");
		} else if (file_db_version < DBVersion) {
			std::cerr << "Old datatabase format found, trying to update..." << std::endl;
			UpdateDB(file_db_version);
		} else {
			std::cerr << "Creating tables if necessary" << std::endl;
			CreateTables();
		}
	}



	std::cerr << "Initializing counter caches" << std::endl;
	InitCounterCaches();

	std::cerr << "Getting internal ids for devices and channels" << std::endl;
	InitDeviceIds();
	InitChannelIds();

	std::cerr << "Getting and assigning group ids" << std::endl;
	InitGroupIds();

    std::cerr << "Analyzing data table" << std::endl;
    DB->exec("ANALYZE data");
    DB->exec("ANALYZE sqlite_master");

    std::cerr << "DB initialization is done" << std::endl;
}

int TMQTTDBLogger::GetOrCreateChannelId(const TChannel & channel)
{
	auto it = ChannelIds.find(channel);
	if (it != ChannelIds.end()) {
		return it->second;
	} else {

		static SQLite::Statement query(*DB, "INSERT INTO channels (device, control) VALUES (?, ?) ");

		query.reset();
		query.bind(1, channel.Device);
		query.bind(2, channel.Control);

		query.exec();

		int channel_id = DB->getLastInsertRowid();
		ChannelIds[channel] = channel_id;
		return channel_id;
	}
}

int TMQTTDBLogger::GetOrCreateDeviceId(const string& device)
{
	auto it = DeviceIds.find(device);
	if (it != DeviceIds.end()) {
		return it->second;
	} else {

		static SQLite::Statement query(*DB, "INSERT INTO devices (device) VALUES (?) ");

		query.reset();
		query.bind(1, device);

		query.exec();

		int device_id = DB->getLastInsertRowid();
		DeviceIds[device] = device_id;
		return device_id;
	}
}

bool TMQTTDBLogger::UpdateAccumulator(int channel_id, const string &payload)
{
    double value = 0.0;

    // try to cast value to double and run stats
    try {
        value = stod(payload);
    } catch (...) {
        return false; // no processing for uncastable values
    }

    auto& accum = ChannelAccumulator[channel_id];
    int& num_values = get<0>(accum);
    double& sum = get<1>(accum);
    double& min = get<2>(accum);
    double& max = get<3>(accum);

    num_values++;

    if (num_values == 1) {
        min = max = sum = value;
    } else {
        if (min > value)
            min = value;
        if (max < value)
            max = value;
        sum += value;
    }
    
    return true;
}


void TMQTTDBLogger::OnConnect(int rc){
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

void TMQTTDBLogger::Init2()
{
    RPCServer = make_shared<TMQTTRPCServer>(shared_from_this(), "db_logger");
    RPCServer->RegisterMethod("history", "get_values", std::bind(&TMQTTDBLogger::GetValues, this, placeholders::_1));
    RPCServer->RegisterMethod("history", "get_channels", std::bind(&TMQTTDBLogger::GetChannels, this, placeholders::_1));
    RPCServer->Init();
}

Json::Value TMQTTDBLogger::GetValues(const Json::Value& params)
{
    cout << "run method " << endl;
    high_resolution_clock::time_point t1 = high_resolution_clock::now();

    Json::Value result;
    int limit = -1;
    double timestamp_gt = 0;
    int64_t uid_gt = -1;
    double timestamp_lt = 10675199167; // magic?
	int req_ver = 0;
	int min_interval_ms = 0;

	if (params.isMember("ver")) {
		req_ver = params["ver"].asInt();
	}

	if ((req_ver != 0) && (req_ver != 1)) {
		throw TBaseException("unsupported request version");
	}

    if (params.isMember("timestamp")) {
        if (params["timestamp"].isMember("gt"))
            timestamp_gt = params["timestamp"]["gt"].asDouble();

        if (params["timestamp"].isMember("lt"))
            timestamp_lt = params["timestamp"]["lt"].asDouble();
    }

    if (params.isMember("uid")) {
        if (params["uid"].isMember("gt")) {
            uid_gt = params["uid"]["gt"].asInt64();
        }
    }

    if (params.isMember("limit"))
        limit = params["limit"].asInt();

    if (params.isMember("min_interval")) {
        min_interval_ms = params["min_interval"].asInt();
        if (min_interval_ms < 0) {
			min_interval_ms = 0;
		}
	}



    if (! params.isMember("channels"))
        throw TBaseException("no channels specified");

    result["values"] = Json::Value(Json::arrayValue);

    // version 3.7 can't always figure out to use the proper index
    string get_values_query_str;

    if (min_interval_ms > 0)
        get_values_query_str = "SELECT uid, device, channel, AVG(value), (timestamp - 2440587.5)*86400.0, min, max  FROM data INDEXED BY data_topic_timestamp WHERE ";
    else
        get_values_query_str = "SELECT uid, device, channel, value, (timestamp - 2440587.5)*86400.0, min, max  FROM data INDEXED BY data_topic_timestamp WHERE ";

    if (!params["channels"].empty()) {
        get_values_query_str += "channel IN ( ";
        for (size_t i = 0; i < params["channels"].size(); ++i) {
            if (i > 0) 
                get_values_query_str += ", ";

            get_values_query_str += "?";
        }
        get_values_query_str += ") AND ";
    }

    get_values_query_str += "timestamp > julianday(datetime(?,'unixepoch')) AND timestamp < julianday(datetime(?,'unixepoch')) AND uid > ? ";


	if (min_interval_ms > 0) {
		get_values_query_str +=  " GROUP BY ROUND( timestamp * ?) ";
	}

	get_values_query_str += " ORDER BY uid ASC LIMIT ?";

    SQLite::Statement get_values_query(*DB, get_values_query_str);
    get_values_query.reset();

    int param_num = 0;
	std::map<int,int> query_channel_ids; // map channel ids to they serial number in the request
	std::map<int, TChannel> channel_names; // map channel ids to the their names  ((device, control) pairs)
	size_t i = 0;
    for (const auto& channel_item : params["channels"]) {
        if (!(channel_item.isArray() && (channel_item.size() == 2)))
            throw TBaseException("'channels' items must be an arrays of size two ");

        const TChannel channel = {channel_item[0u].asString(), channel_item[1u].asString()};

		int channel_int_id = GetOrCreateChannelId(channel);

        get_values_query.bind(++param_num, channel_int_id);

        query_channel_ids[channel_int_id] = (i++);
        channel_names[channel_int_id] = channel;
    }

    get_values_query.bind(++param_num, timestamp_gt);
    get_values_query.bind(++param_num, timestamp_lt);
    get_values_query.bind(++param_num, static_cast<sqlite3_int64>(uid_gt));

	if (min_interval_ms > 0) {
		double day_fraction =   86400000. / min_interval_ms /* ms in day */;
		cout << "day: fraction :" << day_fraction << endl;
		get_values_query.bind(++param_num, day_fraction);
	}

    get_values_query.bind(++param_num, limit + 1); // we request one extra row to know whether there are more than 'limit' available

    int row_count = 0;
    bool has_more = false;

    while (get_values_query.executeStep()) {
        if (row_count >= limit) {
            has_more = true;
            break;
        }

        Json::Value row;
        row[(req_ver == 1) ? "i" : "uid"] = static_cast<int>(get_values_query.getColumn(0));

        if (req_ver == 0) {
			const TChannel& channel = channel_names[get_values_query.getColumn(2)];
			row["device"] = channel.Device;
			row["control"] = channel.Control;
		} else if (req_ver == 1) {
			row["c"] = query_channel_ids[get_values_query.getColumn(2)];
		}

        
        // if there are min and max values, send'em too
        if (get_values_query.getColumn(5).getType() != SQLITE_NULL && req_ver != 1) {
            row["value"] = static_cast<double>(get_values_query.getColumn(3));
            row["min"] = static_cast<double>(get_values_query.getColumn(5));
            row["max"] = static_cast<double>(get_values_query.getColumn(6));
        } else {
            row[(req_ver == 1) ? "v" : "value"] = get_values_query.getColumn(3).getText();
        }

        row[(req_ver == 1) ? "t" : "timestamp"] = static_cast<double>(get_values_query.getColumn(4));
        result["values"].append(row);
        row_count += 1;
    }


    if (has_more) {
        result["has_more"] = true;
    }

    
    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();

    cout << "get_values() took " << duration << "ms" << endl;
    
    return result;
}

Json::Value TMQTTDBLogger::GetChannels(const Json::Value& params)
{

    Json::Value result;
    
    high_resolution_clock::time_point t1 = high_resolution_clock::now(); //FIXME: debug

    // get channel names list
    string channels_list_query_str = "SELECT int_id, device, control FROM channels";
    SQLite::Statement channels_list_query(*DB, channels_list_query_str);

    while (channels_list_query.executeStep()) {
        
        /* generate header string */
        string device_name = static_cast<const char *>(channels_list_query.getColumn(1));
        device_name += "/";
        device_name += static_cast<const char *>(channels_list_query.getColumn(2));

        int channel_id = channels_list_query.getColumn(0);

        Json::Value values;
        values["items"] = ChannelRowNumberCache[channel_id];
        values["last_ts"] = duration_cast<seconds>(LastSavedTimestamps[channel_id].time_since_epoch()).count();

        result["channels"][device_name] = values;
    }

    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();

    cout << "RPC request took " << duration << "ms" << endl;

    return result;
}

/* For debugging reasons */
ostream& operator<<(ostream &str, const TLoggingGroup &group)
{
    str << "> Group " << group.Id << " (" << group.IntId << ")" << endl;
    str << ">\tChannels: ";

    for (const TLoggingChannel &ch: group.Channels) {
        str << ch.Pattern << " ";
    }

    str << endl;

    str << ">\tValues: " << group.Values << endl;
    str << ">\tValues total: " << group.ValuesTotal << endl;
    str << ">\tMinInterval: " << group.MinInterval << endl;
    str << ">\tMinUnchangedInterval: " << group.MinUnchangedInterval << endl;

    return str;
}

int main (int argc, char *argv[])
{
    int rc;
    TMQTTDBLogger::TConfig mqtt_config;
    mqtt_config.Host = "localhost";
    mqtt_config.Port = 1883;
    string config_fname;
    int c;

    while ((c = getopt(argc, argv, "hp:H:c:T:")) != -1) {
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
                printf("Usage:\n wb-mqtt-db [options] [mask]\n");
                printf("Options:\n");
                printf("\t-p PORT   \t\t\t set to what port wb-mqtt-db should connect (default: 1883)\n");
                printf("\t-H IP     \t\t\t set to what IP wb-mqtt-db should connect (default: localhost)\n");
                printf("\t-c config     \t\t\t config file\n");

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


        // for(auto group_it = root["groups"].begin(); group_it !=root["groups"].end(); ++group_it) {
            // const auto & group_item = *group_it;
        
        for (const auto& group_item : root["groups"]) {

            TLoggingGroup group;

            if (! group_item.isMember("name")) {
                throw TBaseException("no name specified for group");
            }
            group.Id = group_item["name"].asString();

            if (! group_item.isMember("channels")) {
                throw TBaseException("no channels specified for group");
            }

            for (const auto & channel_item : group_item["channels"]) {
                // convert channel format from d/c to /devices/d/controls/c
                auto name_split = StringSplit(channel_item.asString(), '/');
                TLoggingChannel channel = { "/devices/" + name_split[0] + "/controls/" + name_split[1] };
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

            cout << group;
        }
    }




    mosqpp::lib_init();
    std::shared_ptr<TMQTTDBLogger> mqtt_db_logger(new TMQTTDBLogger(mqtt_config, config));
    mqtt_db_logger->Init();
    mqtt_db_logger->Init2();

    /* init SIGINT and SIGTERM handler to exit gracefully */
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    while (running) {

        rc = mqtt_db_logger->loop();

        if (rc != 0) {
            mqtt_db_logger->reconnect();
        }

    }

    mqtt_db_logger->disconnect();

    return 0;
}


/* vim: set ts=4 sw=4: */
