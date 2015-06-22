#include <wbmqtt/utils.h>
#include <wbmqtt/mqtt_wrapper.h>
#include <chrono>
#include <iostream>
#include <string>
#include <ctime>
#include <unistd.h>


using namespace std;

struct TTimeStamperConfig
{
    string Mask;
};

class TMQTTTimeStamper: public TMQTTWrapper
{
    public:
        TMQTTTimeStamper(const TMQTTTimeStamper::TConfig& mqtt_config, TTimeStamperConfig log_config);
        ~TMQTTTimeStamper();

        void OnConnect(int rc);
        void OnMessage(const struct mosquitto_message *message);
        void OnSubscribe(int mid, int qos_count, const int *granted_qos);

    private:
        string Mask;
};

TMQTTTimeStamper::TMQTTTimeStamper (const TMQTTTimeStamper::TConfig& mqtt_config, TTimeStamperConfig log_config)
    : TMQTTWrapper(mqtt_config)
{
    Mask = log_config.Mask;
    Connect();
}

TMQTTTimeStamper::~TMQTTTimeStamper()
{
}

void TMQTTTimeStamper::OnConnect(int rc)
{
    Subscribe(NULL, Mask);
}

void TMQTTTimeStamper::OnSubscribe(int mid, int qos_count, const int *granted_qos)
{
    cout << "subscription succeded\n";
}

void TMQTTTimeStamper::OnMessage(const struct mosquitto_message *message)
{
    string topic = message->topic;
    string payload = static_cast<const char*>(message->payload);
    std::time_t tt = std::time(NULL);
    Publish(NULL, topic + "/meta/ts", to_string(tt), 0, true);
}

int main (int argc, char *argv[])
{
    int rc;
    TTimeStamperConfig log_config;
    TMQTTTimeStamper::TConfig mqtt_config;
    mqtt_config.Host = "localhost";
    mqtt_config.Port = 1883;
    int c;

    while ((c = getopt(argc, argv, "hp:H:")) != -1) {
        switch(c) {
            case 'p' :
                printf ("Option p with value '%s'\n", optarg);
                mqtt_config.Port = stoi(optarg);
                break;
            case 'H' :
                printf ("Option H with value '%s'\n", optarg);
                mqtt_config.Host = optarg;
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

    if (optind == argc) {
        log_config.Mask = "/devices/+/controls/+";
    } else {
        log_config.Mask = argv[optind];
    }

    mosqpp::lib_init();
    std::shared_ptr<TMQTTTimeStamper> mqtt_stamper(new TMQTTTimeStamper(mqtt_config, log_config));
    mqtt_stamper->Init();

    while(1) {
        rc = mqtt_stamper->loop();
        if (rc != 0) {
            mqtt_stamper->reconnect();
        }
    }
    return 0;
}
