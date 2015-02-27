#include"common/utils.h"
#include"common/mqtt_wrapper.h"
#include<chrono>
#include<getopt.h>
#include<iostream>
#include<fstream>
#include<string>
#include<exception>
#include<stdexcept>
#include<mosquittopp.h>
#include<ctime>
#include<stdio.h>
#include<unistd.h>
#include<sstream>

using namespace std;
class  TLoggerConfig{// class for parsing config
    public:
        inline void SetPath(string s) { Path_to_log = s; }
        inline string GetPath() { return Path_to_log; }
        inline void SetSize(string s) { SetIntFromString(Size, s); };
        inline int GetSize() { return Size; }
        inline void SetMask(string s) { Mask = s; }
        inline string GetMask() { return Mask; }
        static void SetIntFromString(int& i, string s);//auxiliary function to check if input int option is correct
        inline int GetNumber() { return Number; }
        inline void SetNumber(string s) { SetIntFromString(Number, s); }
        void SetOption(string key, string value);// set option key to value
    
    
    private: 
        string Path_to_log;
        int Size;
        string Mask;
        int Number;
};

void TLoggerConfig::SetIntFromString(int& i, string s ){
    try {
        if (s != "")  
            i = stoi(s.c_str());
    }catch (const std::invalid_argument& argument) {
            cerr << "invalid number " << s << endl;
            exit(-1);
    }catch (const std::out_of_range& argument){
            cerr << "out of range " << s << endl;
            exit(-1);
        }

}

void TLoggerConfig::SetOption( string key, string value){
    if (key == "n") {
        SetIntFromString(Number, value);
        return;
    }
    if (key == "s"){
        SetIntFromString(Size, value);
        return;
    }
    if (key == "f"){
        SetPath(value);
        return;
    }
    cerr << "incorrect option " << key << " in config file\n";
}
class MQTTLogger: public TMQTTWrapper 
{
    public: 
        MQTTLogger (const MQTTLogger::TConfig& mqtt_config, TLoggerConfig log_config);
        ~MQTTLogger();

        void OnConnect(int rc) ;
        void OnMessage(const struct mosquitto_message *message);
        void OnSubscribe(int mid, int qos_count, const int *granted_qos);
        
    
    private:
        string Path_To_Log;// path to log file
        int Max;// set maximum size of log file
        unsigned int Timeout;// timeout after which we will do dump logs
        bool FullDump;// if full_dump yes we doing full dump after timeout 
        ofstream Output;
        int Number; // number of old log files;
        string Mask;// mask wich subscribe to 
};

MQTTLogger::MQTTLogger ( const MQTTLogger::TConfig& mqtt_config, TLoggerConfig log_config)
    : TMQTTWrapper(mqtt_config)
{
    Path_To_Log = log_config.GetPath();
    Output.open(Path_To_Log, std::fstream::app);
    if (!Output.is_open()){
        cerr << "Cannot open log file for writting " << Path_To_Log << endl;
        exit (-1);
    }
    Max = 1024 * log_config.GetSize();// set the maximum size of log file
    Number = log_config.GetNumber(); // set the number of old log files
    Mask = log_config.GetMask();// set the mask which subscribe to 
    Connect();
}

MQTTLogger::~MQTTLogger() {}

void MQTTLogger::OnConnect(int rc){
    Subscribe(NULL, Mask);  
}

void MQTTLogger::OnSubscribe(int mid, int qos_count, const int *granted_qos){
    cout << "subscription succeded\n";
}
void MQTTLogger::OnMessage(const struct mosquitto_message *message){
    string topic = message->topic;
    string payload = static_cast<const char*>(message->payload);
    std::chrono::system_clock::time_point current_time = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(current_time);

    Output << topic + " " +  payload << ctime( &tt) << endl;
    if (Output.tellp() > Max){
        Output.close();
        int i;
        for (i = Number-1; i > 0; i--){
            if (access((Path_To_Log + "." + to_string(i)).c_str(), F_OK) != -1){// check if old log file exists
                if (rename((Path_To_Log + "." + to_string(i)).c_str(), (Path_To_Log + "." + to_string(i+1)).c_str()) != 0){
                    cerr << "can't create old log file \n";
                    exit(-1);
            }
        }
        }
        if (rename(Path_To_Log.c_str(), (Path_To_Log + ".1").c_str()) != 0){
            cerr << "can't create old log file \n";
            exit(-1);
        }
        Output.open(Path_To_Log);
        if (!Output.is_open()){
            cerr << "Cannot open log file for writting " << Path_To_Log << endl;
            exit (-1);
        }
        mosquittopp::unsubscribe(NULL, Mask.c_str());// unsubscribe and subscribe to save all retained messages after rotate
        Subscribe(NULL, Mask);  
    }
}

int main (int argc, char* argv[])
{
    int rc;
    TLoggerConfig log_config;
    MQTTLogger::TConfig mqtt_config;
    mqtt_config.Host = "localhost";
    mqtt_config.Port = 1883;
    int c;
    log_config.SetPath("/var/log/mqtt.log");
    log_config.SetSize("200");
    log_config.SetNumber("2");
    
    while ( (c = getopt(argc, argv, "hp:H:s:f:n:") ) != -1 ){
        switch(c){
            case 'n':
                printf ("option n with value '%s'\n", optarg);
                log_config.SetNumber(string(optarg));
                break;
            case 'f':
                printf ("option f with value '%s'\n", optarg);
                log_config.SetPath(string(optarg));
                break;
            case 'p':
                printf ( "option p with value '%s'\n",optarg);
                TLoggerConfig::SetIntFromString(mqtt_config.Port, string(optarg));
                break;
            case 'H':
                printf ("option h with value '%s'\n", optarg);
                mqtt_config.Host = optarg;
                break;
            case 's':
                printf ("option s with value '%s'\n",optarg);
                log_config.SetSize(string(optarg));
                break;
            case '?':
                printf ("?? Getopt returned character code 0%o ??\n",c);
            case 'h':
                printf ( "help menu\n");
            default:
                printf("Usage:\n mqtt_logger [options] mask\n");
                printf("Options:\n");
                printf("\t-n NUMBER \t\t\t Number of old log files to remain(default 2) \n");
                printf("\t-p PORT   \t\t\t set to what port mqtt_logger should connect (default: 1883)\n");
                printf("\t-H IP     \t\t\t set to what IP mqtt_logger should connect (default: localhost)\n");
                printf("\t-s SIZE   \t\t\t Max size (KB) before rotation ( default: 200KB)\n");
                printf("\t-f FILE   \t\t\t File where log will be writting to(default /var/log/mqtt-logger.log)\n"); 
                printf("\tmask      \t\t\t Mask, what topics subscribe to(default: /#)\n");
                return 0;
        }
    }
    if (optind == argc ){
        printf("too few arguments, Where is mask?\n");
        return -1;
    }
    log_config.SetMask(string (argv[optind]));
    if (optind +1 < argc ) {
        printf("too many arguments\n");
        return -1;
    }
    mosqpp::lib_init();
    std::shared_ptr<MQTTLogger> mqtt_logger(new MQTTLogger(mqtt_config, log_config));
    mqtt_logger->Init();
    while (1){
        rc = mqtt_logger->loop();
        //std::chrono::steady_clock::time_point previous_time = std::chrono::steady_clock::now();
        if (rc != 0)
            mqtt_logger->reconnect();
    }

    return 0;
}

