#include "dblogger.h"

#include <iostream>
#include <string>
#include <ctime>
#include <unistd.h>
#include <fstream>
#include <signal.h>
#include <cstdlib>

#include <log4cpp/Category.hh>
#include <log4cpp/RollingFileAppender.hh>
#include <log4cpp/SyslogAppender.hh>
#include <log4cpp/PatternLayout.hh>
#include <log4cpp/OstreamAppender.hh>

#include "logging.h"

using namespace std;
using namespace std::chrono;

static sig_atomic_t running = 1;

/* it handles only SIGTERM and SIGINT to exit gracefully */
void sig_handler(int signal)
{
    running = 0;
}
        
/* For debugging reasons */
ostream& operator<<(ostream &str, const TLoggingGroup &group)
{
    str << "> Group " << group.Id << endl;
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

TMQTTDBLoggerConfig ParseConfigFile(Json::Value &root)
{
    TMQTTDBLoggerConfig config;

    if (!root.isMember("database")) {
        throw TBaseException("database location should be specified in config");
    }

    config.DBFile = root["database"].asString();
    config.Debug = root["debug"].asBool();

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
            if (group_item["values"].asInt() < 0) {
                throw TBaseException("'values' must be positive or zero");
            }
            group.Values = group_item["values"].asInt();
        }

        if (group_item.isMember("values_total")) {
            if (group_item["values_total"].asInt() < 0) {
                throw TBaseException("'values_total' must be positive or zero");
            }
            group.ValuesTotal = group_item["values_total"].asInt();
        }

        if (group_item.isMember("min_interval")) {
            if (group_item["min_interval"].asInt() < 0) {
                throw TBaseException("'min_interval' must be positive or zero");
            }
            group.MinInterval = group_item["min_interval"].asInt();
        }

        if (group_item.isMember("min_unchanged_interval")) {
            if (group_item["min_unchanged_interval"].asInt() < 0)
                throw TBaseException("'min_unchanged_interval' must be positive or zero");
            group.MinUnchangedInterval = group_item["min_unchanged_interval"].asInt();
        }

        config.Groups.push_back(group);
    }

    return config;
}

int main (int argc, char *argv[])
{
    int rc;
    TMQTTDBLogger::TConfig mqtt_config;
    mqtt_config.Host = "localhost";
    mqtt_config.Port = 1883;
    string config_fname;
    int c;
    int verbose_level = 0;

    while ((c = getopt(argc, argv, "hp:H:c:T:v")) != -1) {
        switch (c) {
        case 'p' :
            /* VLOG(2) << "Option p with value " << optarg; */
            mqtt_config.Port = stoi(optarg);
            break;
        case 'H' :
            /* VLOG(2) << "Option H with value " << optarg; */
            mqtt_config.Host = optarg;
            break;

        case 'c':
            /* VLOG(2) << "Option c with value " << optarg; */
            config_fname = optarg;
            break;

        case 'v':
            /* VLOG(2) << "Option v" << optarg; */
            verbose_level++;
            break;

        case '?':
            /* LOG(WARNING) << "?? Getopt returned character code 0%o ??" << static_cast<char>(c); */
        case 'h':
            printf("help menu\n");
        default:
            printf("Usage:\n wb-mqtt-db [options] [mask]\n");
            printf("Options:\n");
            printf("\t-p PORT   \t\t\t set to what port wb-mqtt-db should connect (default: 1883)\n");
            printf("\t-H IP     \t\t\t set to what IP wb-mqtt-db should connect (default: localhost)\n");
            printf("\t-c config \t\t\t config file\n");
            printf("\t-v        \t\t\t verbose output to stderr (may be -v -v or -v -v -v also)\n");

            return 0;
        }
    }

<<<<<<< HEAD
    // configure logging
    if (verbose_level >= 0) {
        FLAGS_logtostderr = 1;
        FLAGS_v = verbose_level;
    }

=======
>>>>>>> 345ab1bcf5c4d581c055cb29a08c18b05d26bf77


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
                   << reader.getFormatedErrorMessages();

        return 1;
    }

    try {
        config = ParseConfigFile(root);
    } catch (TBaseException &e) {
        cerr << "Failed to parse config file: " << e.what();

        return 1;
    }

    // configure logging
    log4cpp::Category &log_root = log4cpp::Category::getRoot();

    const char* log_file = getenv("MQTT_DB_LOGFILE");
    if (!log_file) 
        log_file = "/var/log/wirenboard/wb-mqtt-db.log";

    int max_file_size = 1; // in MBytes
    const char *env_max_file_size = getenv("MQTT_DB_MAX_LOGFILE_SIZE");
    if (env_max_file_size)
        max_file_size = atoi(env_max_file_size);

    if (verbose_level >= 0)
        log_root.setPriority(log4cpp::Priority::INFO);
    
    log4cpp::PatternLayout *log_layout = new log4cpp::PatternLayout;
    log_layout->setConversionPattern("%d{%Y-%m-%d %H:%M:%S.%l} %p: %m%n");

    // Enable huge debug logging if required
    if (verbose_level > 0) {
        auto appender = new log4cpp::OstreamAppender("default", &cerr);

<<<<<<< HEAD
    ::google::InitGoogleLogging(argv[0]);
=======
        appender->setLayout(log_layout);
        log_root.addAppender(appender);

        auto priority = log4cpp::Priority::NOTICE;

        switch (verbose_level) {
        case 1:
            break;
        case 2:
            priority = log4cpp::Priority::INFO;
            break;
        case 3:
            priority = log4cpp::Priority::DEBUG;
            break;
        default:
            break;
        }

        log_root.setPriority(priority);
    } else if (config.Debug) {
        auto appender = new log4cpp::RollingFileAppender("default", log_file, 
                max_file_size * 1024 * 1024);
        appender->setLayout(log_layout);
        log_root.addAppender(appender);
        log_root.setPriority(log4cpp::Priority::INFO);
    } else {
        // default appender is Syslog appender - not for debug use!
        long pid = getpid();
        auto appender = new log4cpp::SyslogAppender("syslog", "wb-mqtt-db[" + to_string(pid) + "]");
        appender->setLayout(log_layout);
        log_root.addAppender(appender);
        log_root.setPriority(log4cpp::Priority::NOTICE);
    }
>>>>>>> 345ab1bcf5c4d581c055cb29a08c18b05d26bf77

    mosqpp::lib_init();
    std::shared_ptr<TMQTTDBLogger> mqtt_db_logger(new TMQTTDBLogger(mqtt_config, config));

    try {
        mqtt_db_logger->Init();
        mqtt_db_logger->Init2();
    } catch (TBaseException &e) {
        LOG(ERROR) << "Failed to init logger: " << e.what();

        return 2;
    }

    /* init SIGINT and SIGTERM handler to exit gracefully */
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);


    steady_clock::time_point next_call = steady_clock::now();

    SYSLOG(NOTICE) << "DB logger started, go to main loop";

    while (running) {
        /* process MQTT events */
        rc = mqtt_db_logger->loop(duration_cast<milliseconds>(next_call - steady_clock::now()).count());

        if (rc != 0) 
            mqtt_db_logger->reconnect();

        /* process timer events */
        next_call = mqtt_db_logger->ProcessTimer(next_call);
    }

    SYSLOG(NOTICE) << "Exit signal received, stopping";

    mqtt_db_logger->disconnect();

    return 0;
}
