#include <iostream>
#include <unistd.h>
#include "Razberry.h"

using namespace std;

int main (int argc, char **argv) 
{
    int c;
    int port = 0;
    string address = "";
    while ((c = getopt(argc, argv, "hp:H:")) != -1)
    {
        switch (c) {
            case 'p' : 
                printf ("Option p with value %s\n", optarg);
                port = stoi(optarg);
                break;
            case 'H' :
                printf ("Option H with value %s\n", optarg);
                address = optarg;
                break;
            case 'h' :
                printf("Help menu\n");
            default :
                printf("Usage:\n wb-zway [options] \n");
                printf("Options\n");
                printf("\t-p PORT \t\t\t set what port wb-zway daemon should connect to\n");
                printf("\t-H HOST \t\t\t set host address wb-zway daemon shoult connect to\n");
                return 0;
        }
    }
    TMQTTZWay::TConfig mqtt_config;
    int rc;
    mqtt_config.Host = "localhost";
    mqtt_config.Port = 1883;
    TZWayConfig zway_config;
    zway_config.Address = (address != "") ? address: "46.8.48.200";
    if (port != 0) 
        zway_config.Port = port;
    mosqpp::lib_init();
    std::shared_ptr<TMQTTZWay> zway_handler(new TMQTTZWay(mqtt_config, zway_config));
    zway_handler->Init();
    while (1) {
        rc = zway_handler->loop();
        if (rc != 0) 
            zway_handler->reconnect();
        else 
            zway_handler->UpdateValues();
    }
    mosqpp::lib_cleanup();
    return 0;
}
