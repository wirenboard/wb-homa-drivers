#include "dblogger.h"

#include <iostream>
#include <string>
#include <ctime>
#include <unistd.h>
#include <fstream>
#include <signal.h>

#include <glog/logging.h>

using namespace std;

static sig_atomic_t running = 1;

/* it handles only SIGTERM and SIGINT to exit gracefully */
void sig_handler(int signal)
{
    running = 0;
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

    google::InitGoogleLogging(argv[0]);

    while ((c = getopt(argc, argv, "hp:H:c:T:")) != -1) {
        switch (c) {
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

    // Let's parse config
    Json::Value root;
    Json::Reader reader;
    std::ifstream configStream(config_fname);
    bool parsedSuccess = reader.parse(configStream,
                                   root,
                                   false);

    if (not parsedSuccess)
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

    for (const auto& group_item : root["groups"]) {

        TLoggingGroup group;

        if (!group_item.isMember("name")) {
            throw TBaseException("no name specified for group");
        }
        group.Id = group_item["name"].asString();

        if (!group_item.isMember("channels")) {
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

    cout << "Exit signal received, stopping..." << endl;

    mqtt_db_logger->disconnect();

    return 0;
}
