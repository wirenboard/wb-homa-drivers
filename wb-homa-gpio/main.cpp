#include <iostream>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>

#include <getopt.h>

// This is the JSON header
#include "jsoncpp/json/json.h"

#include <mosquittopp.h>

#include "sysfs_gpio.h"
#include <utils.h>
#include <mqtt_wrapper.h>
#include<chrono>
#include<thread>


using namespace std;
using  std::chrono::duration_cast;
using  std::chrono::milliseconds;
using  std::chrono::steady_clock;

enum class TGpioDirection {
    Input,
    Output
};

struct TGpioDesc
{
    int Gpio;
    bool Inverted = false;
    string Name = "";
    TGpioDirection Direction = TGpioDirection::Output;
    string InterruptEdge = "";
    string Type = "";
    int Multiplier;
    int Order;
};


class THandlerConfig
{
    public:
        vector<TGpioDesc> Gpios;
        void AddGpio(TGpioDesc& gpio_desc) { Gpios.push_back(gpio_desc); };

        string DeviceName;

};

typedef pair<TGpioDesc, std::shared_ptr<TSysfsGpio>> TChannelDesc;

bool FuncComp( const TChannelDesc& a, const TChannelDesc& b)
    { return (a.first.Order < b.first.Order); }
class TMQTTGpioHandler : public TMQTTWrapper
{

	public:
        TMQTTGpioHandler(const TMQTTGpioHandler::TConfig& mqtt_config, const THandlerConfig& handler_config);
		~TMQTTGpioHandler();

		void OnConnect(int rc);
		void OnMessage(const struct mosquitto_message *message);
		void OnSubscribe(int mid, int qos_count, const int *granted_qos);

        void UpdateChannelValues();
        void InitInterrupts(int epfd);// look through all gpios and select Interrupt supporting ones
        string GetChannelTopic(const TGpioDesc& gpio_desc);
        void CatchInterrupts(int count, struct epoll_event* events);
        void PublishValue(const TGpioDesc& gpio_desc, std::shared_ptr<TSysfsGpio> gpio_handler);
        bool FirstTime = true;

    private:
        THandlerConfig Config;
        vector<TChannelDesc> Channels;

        void UpdateValue(const TGpioDesc& gpio_desc,std::shared_ptr<TSysfsGpio> gpio_handler);

};







TMQTTGpioHandler::TMQTTGpioHandler(const TMQTTGpioHandler::TConfig& mqtt_config, const THandlerConfig& handler_config)
    : TMQTTWrapper(mqtt_config)
    , Config(handler_config)
{
    // init gpios
    for (const TGpioDesc& gpio_desc : handler_config.Gpios) {
        std::shared_ptr<TSysfsGpio> gpio_handler(nullptr);
        if (gpio_desc.Type == "")
                gpio_handler.reset( new TSysfsGpio(gpio_desc.Gpio, gpio_desc.Inverted, gpio_desc.InterruptEdge));
        else
            gpio_handler.reset( new TSysfsGpioBaseCounter(gpio_desc.Gpio, gpio_desc.Inverted, gpio_desc.InterruptEdge,  gpio_desc.Type, gpio_desc.Multiplier));
        gpio_handler->Export();
        if (gpio_handler->IsExported()) {
            if (gpio_desc.Direction == TGpioDirection::Input)
                gpio_handler->SetInput();
            else
                gpio_handler->SetOutput();
            Channels.emplace_back(gpio_desc, gpio_handler);
            } else {
                    cerr << "ERROR: unable to export gpio " << gpio_desc.Gpio << endl;
            }
    }
    sort(Channels.begin(), Channels.end(), FuncComp);
	Connect();
}

TMQTTGpioHandler::~TMQTTGpioHandler() {}

void TMQTTGpioHandler::OnConnect(int rc)
{
	printf("Connected with code %d.\n", rc);
	if(rc == 0){
		/* Only attempt to Subscribe on a successful connect. */
        string prefix = string("/devices/") + MQTTConfig.Id + "/";

        // Meta
        Publish(NULL, prefix + "meta/name", Config.DeviceName, 0, true);

        for (const auto& channel_desc : Channels) {
            const auto& gpio_desc = channel_desc.first;

            //~ cout << "GPIO: " << gpio_desc.Name << endl;
            string control_prefix = prefix + "controls/" + gpio_desc.Name;
            std::shared_ptr<TSysfsGpio> gpio_handler = channel_desc.second;
            vector<TPublishPair> what_to_publish (gpio_handler->MetaType());
            int order = gpio_desc.Order * 2;
            for (TPublishPair tmp : what_to_publish) {
                Publish(NULL, control_prefix + "/meta/order", to_string(order), 0, true);
                Publish(NULL, control_prefix + tmp.first + "/meta/type", tmp.second, 0, true);
                order++;
            }
            if (gpio_desc.Direction == TGpioDirection::Input)
                Publish(NULL, control_prefix + "/meta/readonly", "1", 0, true);
            else
                Subscribe(NULL, control_prefix + "/on");
        }
//~ /devices/293723-demo/controls/Demo-Switch 0
//~ /devices/293723-demo/controls/Demo-Switch/on 1
//~ /devices/293723-demo/controls/Demo-Switch/meta/type switch



	}
}

void TMQTTGpioHandler::OnMessage(const struct mosquitto_message *message)
{
    string topic = message->topic;
    string payload = static_cast<const char *>(message->payload);


    const vector<string>& tokens = StringSplit(topic, '/');

    if (  (tokens.size() == 6) &&
          (tokens[0] == "") && (tokens[1] == "devices") &&
          (tokens[2] == MQTTConfig.Id) && (tokens[3] == "controls") &&
          (tokens[5] == "on") )
    {
        for (TChannelDesc& channel_desc : Channels) {
            const auto& gpio_desc = channel_desc.first;
            if (gpio_desc.Direction != TGpioDirection::Output)
                continue;

            if (tokens[4] == gpio_desc.Name) {
                auto& gpio_handler = *channel_desc.second;

                int val = payload == "0" ? 0 : 1;
                if (gpio_handler.SetValue(val) == 0) {
                    // echo, retained
                    Publish(NULL, GetChannelTopic(gpio_desc), payload, 0, true);
                }else {
                    cerr << "DEBUG : couldn't set value" << endl;
                    }
            }
        }
    }
}

void TMQTTGpioHandler::OnSubscribe(int mid, int qos_count, const int *granted_qos)
{
	printf("Subscription succeeded.\n");
}

string TMQTTGpioHandler::GetChannelTopic(const TGpioDesc& gpio_desc)
{
    static string controls_prefix = string("/devices/") + MQTTConfig.Id + "/controls/";
    return (controls_prefix + gpio_desc.Name);
}

void TMQTTGpioHandler::UpdateValue(const TGpioDesc& gpio_desc, std::shared_ptr<TSysfsGpio> gpio_handler)
{
        // look at previous value and compare it with current
        int cached = gpio_handler->GetCachedValue();
        int value = gpio_handler->GetValue();
        if (value >= 0) {
            // Buggy GPIO driver may yield any non-zero number instead of 1,
            // so make sure it's either 1 or 0 here.
            // See https://github.com/torvalds/linux/commit/25b35da7f4cce82271859f1b6eabd9f3bd41a2bb
            value = !!value;
            if ((cached < 0) || (cached != value)){
                gpio_handler->SetCachedValue(cached);
                PublishValue(gpio_desc, gpio_handler);
                }
            }
}
void TMQTTGpioHandler::PublishValue(const TGpioDesc& gpio_desc, std::shared_ptr<TSysfsGpio> gpio_handler)
{
    vector<TPublishPair> what_to_publish(gpio_handler->GpioPublish()); //gets nessesary to publish with updating value
    for (TPublishPair& publish_element: what_to_publish){
        string to_topic = publish_element.first;
        string value = publish_element.second;
        Publish(NULL, GetChannelTopic(gpio_desc) + to_topic, value, 0, true); // Publish current value (make retained)
        }
}
void TMQTTGpioHandler::UpdateChannelValues()
{
    for (TChannelDesc& channel_desc : Channels) {
        const auto& gpio_desc = channel_desc.first;
        std::shared_ptr<TSysfsGpio> gpio_handler = channel_desc.second;
        UpdateValue(gpio_desc,gpio_handler);
    }
}

void TMQTTGpioHandler::InitInterrupts(int epfd)
{
    int n;
    for ( TChannelDesc& channel_desc : Channels) {
        const auto& gpio_desc = channel_desc.first;
        auto& gpio_handler = *channel_desc.second;
        // check if file edge exists and is direction input
        gpio_handler.InterruptUp();
        if (gpio_handler.GetInterruptSupport()) {
             n = epoll_ctl(epfd,EPOLL_CTL_ADD,gpio_handler.GetFileDes(),&gpio_handler.GetEpollStruct());// adding new instance to epoll
            if (n != 0 ) {
                cerr<<"epoll_ctl gained error with GPIO"<<gpio_desc.Gpio<<endl;
            }
        }
    }

}

void TMQTTGpioHandler::CatchInterrupts(int count, struct epoll_event* events)
{
    int i;
    for ( auto& channel_desc : Channels) {
        const auto& gpio_desc = channel_desc.first;
        std::shared_ptr<TSysfsGpio> gpio_handler = channel_desc.second;
        for (i=0; i < count; i++){
            if (gpio_handler->GetFileDes() == events[i].data.fd) {
                if (!gpio_handler->IsDebouncing()){
                    PublishValue(gpio_desc, gpio_handler);
                }
            }
        }
    }
    //std::this_thread::sleep_for(std::chrono::milliseconds(10));//avoid debouncing

}

int main(int argc, char *argv[])
{
	int rc;
    THandlerConfig handler_config;
    TMQTTGpioHandler::TConfig mqtt_config;
    mqtt_config.Host = "localhost";
    mqtt_config.Port = 1883;
    string config_fname;
    int epfd;
    struct epoll_event events[20];

    int c,n;
    //~ int digit_optind = 0;
    //~ int aopt = 0, bopt = 0;
    //~ char *copt = 0, *dopt = 0;
    while ( (c = getopt(argc, argv, "c:h:p:")) != -1) {
        //~ int this_option_optind = optind ? optind : 1;
        switch (c) {
        case 'c':
            printf ("option c with value '%s'\n", optarg);
            config_fname = optarg;
            break;
        case 'p':
            printf ("option p with value '%s'\n", optarg);
            mqtt_config.Port = stoi(optarg);
            break;
        case 'h':
            printf ("option h with value '%s'\n", optarg);
            mqtt_config.Host = optarg;
            break;
        case '?':
            break;
        default:
            printf ("?? Getopt returned character code 0%o ??\n", c);
        }
    }
    //~ if (optind < argc) {
        //~ printf ("non-option ARGV-elements: ");
        //~ while (optind < argc)
            //~ printf ("%s ", argv[optind++]);
        //~ printf ("\n");
    //~ }






    {
        // Let's parse it
        Json::Value root;
        Json::Reader reader;

        if (config_fname.empty()) {
            cerr << "Please specify config file with -c option" << endl;
            return 1;
        }

        ifstream myfile (config_fname);

        bool parsedSuccess = reader.parse(myfile,
                                       root,
                                       false);

        if(not parsedSuccess)
        {
            // Report failures and their locations
            // in the document.
            cerr << "Failed to parse JSON" << endl
               << reader.getFormatedErrorMessages()
               << endl;
            return 1;
        }


        handler_config.DeviceName = root["device_name"].asString();

         // Let's extract the array contained
         // in the root object
        const auto& array = root["channels"];

         // Iterate over sequence elements and
         // print its values
        for(unsigned int index=0; index<array.size();
             ++index)
        {
            const auto& item = array[index];
            TGpioDesc gpio_desc;
            gpio_desc.Gpio = item["gpio"].asInt();
            gpio_desc.Name = item["name"].asString();
            if (item.isMember("inverted"))
                gpio_desc.Inverted = item["inverted"].asBool();
            if (item.isMember("direction") && item["direction"].asString() == "input")
                gpio_desc.Direction = TGpioDirection::Input;
            if (item.isMember("type"))
                    gpio_desc.Type = item["type"].asString();
            if (item.isMember("multiplier"))
                gpio_desc.Multiplier = item["multiplier"].asInt();
            if (item.isMember("edge"))
                gpio_desc.InterruptEdge = item["edge"].asString();
            gpio_desc.Order = index;
            handler_config.AddGpio(gpio_desc);

        }
    }



	mosqpp::lib_init();

    mqtt_config.Id = "wb-gpio";
    std::shared_ptr<TMQTTGpioHandler> mqtt_handler(new TMQTTGpioHandler(mqtt_config, handler_config));
    mqtt_handler->Init();

    rc= mqtt_handler->loop_start();
    if (rc != 0 ) {
        cerr << "couldn't start mosquitto_loop_start ! " << rc << endl;
    } else {
        epfd = epoll_create(1);// creating epoll for Interrupts
        mqtt_handler->InitInterrupts(epfd);
        steady_clock::time_point start;
        int interval;
        start = steady_clock::now();
        while(1) {
            n = epoll_wait(epfd,events,20,500);
            interval = duration_cast<milliseconds>(steady_clock::now() - start).count() ;
            if (interval >= 500 ) {  //checking is it time to look through all gpios
                mqtt_handler->UpdateChannelValues();
                start = steady_clock::now();
            }else {
                if (mqtt_handler->FirstTime && interval == 0) {
                    mqtt_handler->FirstTime = false;
                    continue;
                }
                mqtt_handler->CatchInterrupts( n, events );
            }
        }
	}

	mosqpp::lib_cleanup();

	return 0;
}
//build-dep libmosquittopp-dev libmosquitto-dev
// dep: libjsoncpp0 libmosquittopp libmosquitto


// 2420 2032
// 6008 2348 1972
