#include "dblogger.h"

#include <glog/logging.h>

using namespace std;
using namespace std::chrono;

int TMQTTDBLogger::GetOrCreateChannelId(const TChannelName & channel)
{
    auto it = ChannelIds.find(channel);
    if (it != ChannelIds.end()) {
        return it->second;
    } else {

        VLOG(1) << "Creating channel " << channel;

        static SQLite::Statement query(*DB, "INSERT INTO channels (device, control) VALUES (?, ?) ");

        query.reset();
        query.bind(1, channel.Device);
        query.bind(2, channel.Control);

        query.exec();

        int channel_id = DB->getLastInsertRowid();
        ChannelIds[channel] = channel_id;

        ChannelDataCache[channel_id].Name = channel;

        return channel_id;
    }
}

int TMQTTDBLogger::GetOrCreateDeviceId(const string& device)
{
    auto it = DeviceIds.find(device);
    if (it != DeviceIds.end()) {
        return it->second;
    } else {

        VLOG(1) << "Creating device " << device;

        static SQLite::Statement query(*DB, "INSERT INTO devices (device) VALUES (?) ");

        query.reset();
        query.bind(1, device);

        query.exec();

        int device_id = DB->getLastInsertRowid();
        DeviceIds[device] = device_id;
        return device_id;
    }
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
    VLOG(0) << "Run RPC get_values()";

#ifndef NDEBUG
    high_resolution_clock::time_point t1 = high_resolution_clock::now();
#endif

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
    std::map<int, TChannelName> channel_names; // map channel ids to the their names  ((device, control) pairs)
    size_t i = 0;
    for (const auto& channel_item : params["channels"]) {
        if (!(channel_item.isArray() && (channel_item.size() == 2)))
            throw TBaseException("'channels' items must be an arrays of size two ");

        const TChannelName channel = {channel_item[0u].asString(), channel_item[1u].asString()};

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
            const TChannelName& channel = channel_names[get_values_query.getColumn(2)];
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

#ifndef NDEBUG   
    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();
    
    DVLOG(0) << "get_values() took " << duration << "ms" << endl;
#endif

    
    return result;
}

Json::Value TMQTTDBLogger::GetChannels(const Json::Value& params)
{

    Json::Value result;
    
    VLOG(0) << "Run RPC get_channels()";
#ifndef NDEBUG
    high_resolution_clock::time_point t1 = high_resolution_clock::now();
#endif

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
        values["items"] = ChannelDataCache[channel_id].RowCount;
        values["last_ts"] = duration_cast<seconds>(ChannelDataCache[channel_id].LastProcessed.time_since_epoch()).count();

        result["channels"][device_name] = values;
    }

#ifndef NDEBUG
    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();

    DVLOG(0) << "RPC request took " << duration << "ms" << endl;
#endif

    return result;
}

