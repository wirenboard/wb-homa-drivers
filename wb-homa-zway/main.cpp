#include <iostream>
#include <fstream>
#include <unistd.h>
#include "Razberry.h"

using namespace std;

int main (int argc, char **argv) 
{
    int c;
    int port = 0;
    string host = "";
    string config_fname;
    TMQTTZWay::TConfig mqtt_config;
    TZWayConfig zway_config;
    mqtt_config.Host = "localhost";
    mqtt_config.Port = 1883;
    while ((c = getopt(argc, argv, "c:hp:H:")) != -1)
    {
        switch (c) {
            case 'p' : 
                printf ("Option p with value %s\n", optarg);
                port = stoi(optarg);
                break;
            case 'H' :
                printf ("Option H with value %s\n", optarg);
                host = optarg;
                break;
            case 'h' :
                printf("Help menu\n");
            case 'c' :
                printf ("Option c with value '%s'\n", optarg);
                config_fname = optarg;
                break;
            default :
                printf("Usage:\n wb-zway [options] \n");
                printf("Options\n");
                printf("\t-p PORT \t\t\t set what port wb-zway daemon should connect to\n");
                printf("\t-H HOST \t\t\t set host address wb-zway daemon shoult connect to\n");
                printf("\t-c FILE \t\t\t set config file \n"); 
                return 0;
        }
    }

    {
        // parsing config file 
        Json::Value root;
        Json::Reader reader;

        if (config_fname.empty()) {
            cerr << "Please specify config file with -c option" << endl;
            return 1;
        }
        ifstream myfile(config_fname);

        bool parsed_success = reader.parse(myfile,root,false);

        if (not parsed_success) {
            cerr << "failed to parse JSON" << endl
                << reader.getFormatedErrorMessages()
                << endl;
            return 1;
        }

        if (root.isMember("mqtt_port")) {
            mqtt_config.Port = root["mqtt_port"].asInt();
        }
        if (root.isMember("mqtt_host")) {
            mqtt_config.Host = root["mqtt_host"].asString();
        }
        if (root.isMember("zway_port")) {
            zway_config.Port = root["zway_port"].asInt();
        }
        if (root.isMember("zway_host")) {
            zway_config.Host = root["zway_host"].asString();
        }
        if (root.isMember("zway_username")) {
            zway_config.Username = root["zway_username"].asString();
            zway_config.Password = root["zway_password"].asString();
        }
    }


        
    int rc;
    if (host != "")
        zway_config.Host = host;
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
