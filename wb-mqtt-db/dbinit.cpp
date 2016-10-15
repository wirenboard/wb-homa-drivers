#include "dblogger.h"

#include <logging.h>

using namespace std;
using namespace std::chrono;

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
    LOG(INFO) << "Creating 'devices' table...";
    DB->exec("CREATE TABLE IF NOT EXISTS devices ( "
             "int_id INTEGER PRIMARY KEY AUTOINCREMENT, "
             "device VARCHAR(255) UNIQUE "
             " )  ");

    LOG(INFO) << "Creating 'channels' table...";
    DB->exec("CREATE TABLE IF NOT EXISTS channels ( "
             "int_id INTEGER PRIMARY KEY AUTOINCREMENT, "
             "device VARCHAR(255), "
             "control VARCHAR(255) "
             ")  ");

    LOG(INFO) << "Creating 'groups' table...";
    DB->exec("CREATE TABLE IF NOT EXISTS groups ( "
             "int_id INTEGER PRIMARY KEY AUTOINCREMENT, "
             "group_id VARCHAR(255) "
             ")  ");

    LOG(INFO) << "Creating 'data' table...";
    DB->exec("CREATE TABLE IF NOT EXISTS data ("
             "uid INTEGER PRIMARY KEY AUTOINCREMENT, "
             "device INTEGER,"
             "channel INTEGER,"
             "value VARCHAR(255),"
             "timestamp INTEGER DEFAULT(0),"
             "group_id INTEGER,"
             "max VARCHAR(255),"
             "min VARCHAR(255),"
             "retained INTEGER"
             ")"
            );

    LOG(INFO) << "Creating 'variables' table...";
    DB->exec("CREATE TABLE IF NOT EXISTS variables ("
             "name VARCHAR(255) PRIMARY KEY, "
             "value VARCHAR(255) )"
            );

    LOG(INFO) << "Creating 'data_topic' index on 'data' ('channel')";
    DB->exec("CREATE INDEX IF NOT EXISTS data_topic ON data (channel)");

    // NOTE: the following index is a "low quality" one according to sqlite documentation. However, reversing the order of columns results in factor of two decrease in SELECT performance. So we leave it here as it is. 
    LOG(INFO) << "Creating 'data_topic_timestamp' index on 'data' ('channel', 'timestamp')";
    DB->exec("CREATE INDEX IF NOT EXISTS data_topic_timestamp ON data (channel, timestamp)");

    LOG(INFO) << "Creating 'data_gid' index on 'data' ('group_id')";
    DB->exec("CREATE INDEX IF NOT EXISTS data_gid ON data (group_id)");

    LOG(INFO) << "Creating 'data_gid_timestamp' index on 'data' ('group_id', 'timestamp')";
    DB->exec("CREATE INDEX IF NOT EXISTS data_gid_timestamp ON data (group_id, timestamp)");

    {
        LOG(INFO) << "Updating database version variable...";
        SQLite::Statement query(*DB, "INSERT OR REPLACE INTO variables (name, value) VALUES ('db_version', ?)");
        query.bind(1, DBVersion);
        query.exec();
    }

}

void TMQTTDBLogger::InitCaches()
{
    // init group counter cache
    LOG(INFO) << "Fill GroupRowNumberCache...";
    SQLite::Statement count_group_query(*DB, "SELECT COUNT(*) as cnt, group_id FROM data GROUP BY group_id ");
    while (count_group_query.executeStep()) {
        GroupRowNumberCache[count_group_query.getColumn(1)] = count_group_query.getColumn(0);
    }

    // init channel counter cache
    // init channel last state values
    
    LOG(INFO) << "Fill ChannelDataCache.RowCount, ChannelDataCache.LastProcessed, ChanneldDataCache.LastValue...";
    /* SQLite::Statement count_channel_query(*DB, "SELECT COUNT(rowid) as cnt, MAX(timestamp) AS ts, value, channel \ */
    /*                                             FROM data GROUP BY channel ORDER BY timestamp DESC"); */
    /* while (count_channel_query.executeStep()) { */
    /*     auto& channel_data = ChannelDataCache[count_channel_query.getColumn(3)]; */
        
    /*     auto d = milliseconds(static_cast<long long>(count_channel_query.getColumn(1)) * 1000); */
    /*     auto current_tp = steady_clock::time_point(d); */
        
    /*     channel_data.RowCount = count_channel_query.getColumn(0); */
    /*     channel_data.LastProcessed = current_tp; */
    /*     channel_data.LastValue = static_cast<const char *>(count_channel_query.getColumn(2)); */
    /* } */

    SQLite::Statement count_channel_query(*DB, "SELECT timestamp, value, channel FROM data");

    while (count_channel_query.executeStep()) {
        auto& channel_data = ChannelDataCache[count_channel_query.getColumn(2)];

        // increment RowCount
        channel_data.RowCount++;

        // prepare timestamps
        auto d = milliseconds(static_cast<long long>(count_channel_query.getColumn(1)) * 1000);
        auto current_tp = steady_clock::time_point(d);

        if (current_tp > channel_data.LastProcessed) {
            channel_data.LastProcessed = current_tp;
            channel_data.LastValue = static_cast<const char *>(count_channel_query.getColumn(1));
        }
    }
}

void TMQTTDBLogger::InitChannelIds()
{
    SQLite::Statement query(*DB, "SELECT int_id, device, control FROM channels");
    while (query.executeStep()) {
        int channel_id = query.getColumn(0);
        TChannelName name = { query.getColumn(1), query.getColumn(2) };

        ChannelIds[name] = channel_id;
        ChannelDataCache[channel_id].Name = name;

        // reset values before init caches
        ChannelDataCache[channel_id].LastProcessed = steady_clock::time_point(milliseconds(0));
        ChannelDataCache[channel_id].RowCount = 0;
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

    auto now = steady_clock::now();

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

        group.ChannelIds.clear(); // FIXME
        group.LastSaved = now;
        group.LastUSaved = now;
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

        LOG(INFO) << "Convert database from version 0";

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
        
        DB->exec("UPDATE variables SET value=\"1\" WHERE name=\"db_version\"");

        transaction.commit();

        // defragment database
        DB->exec("VACUUM");

    case 1:
        // In versions >= 2, there is a difference in 'data' table:
        // add data.max, data.min columns
        
        LOG(INFO) << "Convert database from version 1";

        DB->exec("ALTER TABLE data ADD COLUMN max VARCHAR(255)");
        DB->exec("ALTER TABLE data ADD COLUMN min VARCHAR(255)");
        DB->exec("ALTER TABLE data ADD COLUMN retained INTEGER");

        DB->exec("UPDATE data SET max = value");
        DB->exec("UPDATE data SET min = value");
        DB->exec("UPDATE data SET retained = 0");

        DB->exec("UPDATE variables SET value=\"2\" WHERE name=\"db_version\"");

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
        LOG(INFO) << "Creating tables";
        CreateTables();
    } else {
        int file_db_version = ReadDBVersion();
        if (file_db_version > DBVersion) {
            throw TBaseException("Database file is created by newer version of wb-mqtt-db");
        } else if (file_db_version < DBVersion) {
            LOG(WARN) << "Old database format found, trying to update...";
            UpdateDB(file_db_version);
        } else {
            LOG(INFO) << "Creating tables if necessary";
            CreateTables();
        }
    }

    VLOG(0) << "Getting internal ids for devices and channels";
    InitDeviceIds();
    InitChannelIds();

    VLOG(0) << "Initializing counter caches";
    InitCaches();

    VLOG(0) << "Getting and assigning group ids";
    InitGroupIds();

    VLOG(0) << "Analyzing data table";
    DB->exec("ANALYZE data");
    DB->exec("ANALYZE sqlite_master");

    VLOG(0) << "DB initialization is done";
}

