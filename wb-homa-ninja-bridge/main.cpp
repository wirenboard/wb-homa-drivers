#include <iostream>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <getopt.h>
#include <memory>

#include "jsoncpp/json/json.h"
#include <mosquittopp.h>
#include <curl/curl.h>

#include <utils.h>
#include <mqtt_wrapper.h>
#include "http_helper.h"
#include "cloud_connection.h"
#include "local_connection.h"




using namespace std;





int main(int argc, char *argv[])
{
    TMQTTNinjaBridgeHandler::TConfig mqtt_config;
    mqtt_config.Host = "localhost";
    mqtt_config.Port = 1883;

    int c;
    //~ int digit_optind = 0;
    while ( (c = getopt(argc, argv, "c:h:p:")) != -1) {
        //~ int this_option_optind = optind ? optind : 1;
        switch (c) {
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
                printf ("?? getopt returned character code 0%o ??\n", c);
        }
    }


	mosqpp::lib_init();

    mqtt_config.Id = "wb-homa-ninja-bridge";


    //will block until activation
    std::shared_ptr<TMQTTNinjaBridgeHandler> mqtt_handler(new TMQTTNinjaBridgeHandler(mqtt_config));
    mqtt_handler->Init();

    // run MQTT loop for 2 seconds to allow it to publish messages
    mqtt_handler->LoopFor(1000 * 2);


    // will block until we get activation from cloud
    mqtt_handler->WaitForActivation();
    // connect to ninja cloud once we get block token
    mqtt_handler->ConnectToCloud();


    mqtt_handler->LoopForever();
	mosqpp::lib_cleanup();

	return 0;
}
