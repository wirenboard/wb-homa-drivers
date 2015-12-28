#include <cstdlib>
#include <iostream>
#include <fstream>
#include <getopt.h>
#include <string>
#include <errno.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <mutex>
#include <wbmqtt/utils.h>
#include <wbmqtt/mqtt_wrapper.h>
#include "jsoncpp/json/json.h"
#include "lirc/lirc_client.h"

using namespace std;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::steady_clock;

struct TButton
{
	string Name;
	string Remote;
	string Key;
	int Repeat = 1;
};

struct THandlerConfig
{
	string DeviceName = "IR Remote Control";
	bool Debug = false;
	int ReleaseTime = 100;
	int RepeatTime = 100;
	vector<TButton> Buttons;
};

class TLircException: public exception 
{
public:
	TLircException(string _message): message(_message) {}
	const char* what () const throw ()
	{
		return ("LIRC error: " + message).c_str();
	}
	void fatal () const
	{
		cerr << "FATAL: " << what() << endl;
		exit(1);
	}
private:
	string message;
};

void LoadConfig(const string& file_name, THandlerConfig& config)
{
	ifstream config_file (file_name);
	if (config_file.fail())
		return; // just use defaults

	Json::Value root;
	Json::Reader reader;
	bool parsedSuccess = reader.parse(config_file, root, false);

	// Report failures and their locations in the document.
	if(not parsedSuccess)
		throw TLircException("Failed to parse config JSON: " + reader.getFormatedErrorMessages());
	if (!root.isObject())
		throw TLircException("Bad config file (the root is not an object)");
	if (root.isMember("device_name"))
		config.DeviceName = root["device_name"].asString();
	if (root.isMember("debug"))
		config.Debug = root["debug"].asBool();
	if (root.isMember("release_time"))
		config.ReleaseTime = root["release_time"].asInt();
	if (root.isMember("repeat_time"))
		config.RepeatTime = root["repeat_time"].asInt();

	const auto& buttons = root["buttons"];

	for (unsigned int index = 0; index < buttons.size(); index++){
		const auto& item = buttons[index];
		TButton new_button;// create new intrance to add in vector<TChannel> Channels

		if (item.isMember("name")) {
			new_button.Name = item["name"].asString();
		}
		else if (item.isMember("key")) {
			new_button.Name = item["key"].asString();
		}
		else
			throw TLircException("Unable to get button name");

		if (item.isMember("remote")) {
			new_button.Remote = item["remote"].asString();
		}
		else
			throw TLircException("Missing remote name for button");

		if (item.isMember("key")) {
			new_button.Remote = item["remote"].asString();
		}
		else
			throw TLircException("Missing key name for button");

		if (item.isMember("repeat")) {
			new_button.Repeat = item["repeat"].asInt();
		}

		config.Buttons.push_back(new_button);
	}
}

class TMQTTLircHandler : public TMQTTWrapper
{
public:
	TMQTTLircHandler(const TMQTTLircHandler::TConfig& mqtt_config, const THandlerConfig handler_config) ;
	~TMQTTLircHandler();

	void OnConnect(int rc) ;
	void OnMessage(const struct mosquitto_message *message);
	void OnSubscribe(int mid, int qos_count, const int *granted_qos);

	bool Learn = false;

	thread KeyThread;

	void ConnectToLirc();
	void NextCode();

private:
	THandlerConfig Config;
	int fd;
	string key;
	int repeat;
	steady_clock::time_point last_time;
	steady_clock::time_point repeat_time;
	mutex key_mutex;
	void key_thread();
};


TMQTTLircHandler::TMQTTLircHandler(const TMQTTLircHandler::TConfig& mqtt_config, THandlerConfig handler_config)
	: TMQTTWrapper(mqtt_config),
	Config(handler_config)
{
	mosqpp::lib_init();
	KeyThread = thread(&TMQTTLircHandler::key_thread, this);
	Connect();
}

TMQTTLircHandler::~TMQTTLircHandler() {
	lirc_deinit();
	mosqpp::lib_cleanup();
}

void TMQTTLircHandler::OnConnect(int rc)
{
	if (Config.Debug)
		cerr << "Connected with code " << rc << endl;

	if(rc != 0)
		return;

	string path = string("/devices/") + MQTTConfig.Id;
	Publish(NULL, path + "/meta/name", Config.DeviceName.c_str(), 0, true);

	int n = 0;
	Publish(NULL, path + "/controls/Learn/meta/order", to_string(n++), 0, true);
	Publish(NULL, path + "/controls/Learn/meta/type", "switch", 0, true);
	Publish(NULL, path + "/controls/Key/meta/order", to_string(n++), 0, true);
	Publish(NULL, path + "/controls/Key/meta/type", "text", 0, true);
	/*
	for (auto button : Buttons) {
		string topic = path + "/" + button.Name;
		Publish(NULL, topic + "/meta/order", to_string(n++), 0, true);
		Publish(NULL, topic + "/meta/type", "pushbutton", 0, true);
	}
	*/
	Subscribe(NULL, path + "/controls/Key/on");
}

void TMQTTLircHandler::OnMessage(const struct mosquitto_message *message)
{
	string topic = message->topic;
	string payload = static_cast<const char *>(message->payload);

	const vector<string>& tokens = StringSplit(message->topic, '/');
	if ((tokens.size() == 6) &&
			(tokens[0] == "") && (tokens[1] == "devices") &&
			(tokens[2] == MQTTConfig.Id) && (tokens[3] == "controls")) {

		if ((tokens[4] == "Key") && (tokens[5] == "on")) {
			size_t pos = payload.find_first_of(':');
			if (pos == string::npos) {
				if (Config.Debug)
					cerr << "Failed to parse key message: " << payload << endl;
				return;
			}
			string remote = string(payload, 0, pos);
			string key = string(payload, pos+1);
			if (Config.Debug)
				cerr << "Send: remote = " << remote << ", key = " << key << endl;
			if (lirc_send_one(fd, remote.c_str(), key.c_str()) < 0)
				throw TLircException("lirc_send_one() failed: " + string(strerror(errno)));
		}
	}	
}

void TMQTTLircHandler::OnSubscribe(int, int, const int *)
{
	if (Config.Debug)
		cerr << "Subscription succeeded." << endl;
}

void TMQTTLircHandler::ConnectToLirc()
{
	if (Config.Debug)
		cerr << "Connecting to LIRC daemon" << endl;

	lirc_deinit();

	if (lirc_init("wb-lirc", Config.Debug ? 5 : 0) == -1)
		throw TLircException("Couldn't initialize LIRC: " + string(strerror(errno)));

	fd = lirc_get_local_socket(NULL, Config.Debug ? 0 : 1);
	if (fd < 0)
		throw TLircException("Couldn't get LIRC socket: " + string(strerror(-fd)));
}

void TMQTTLircHandler::NextCode()
{
	char *code, *p, *ep;
	if (lirc_nextcode(&code) != 0)
		throw TLircException("lirc_nextcode() failed: " + string(strerror(errno)));

	if (code == NULL) {
		if (Config.Debug)
			cerr << "lirc_nextcode() timeout" << endl;
		return;
	}

	auto ParseFailed = TLircException("Failed to parse LIRC code: " + string(code));

	p = code;

	p = strchr(p, ' ');
	if (!code)
		throw ParseFailed;

	int rpt = strtol(p, &ep, 16);
	if (ep <= p)
		throw ParseFailed;
	
	p = ep + 1;
	ep = strchr(p, ' ');
	if (ep == NULL)
		throw ParseFailed;
	string k = string(p, ep-p);

	p = ep + 1;
	ep = strchr(p, '\n');
	if (ep == NULL)
		throw ParseFailed;
	string remote = string(p, ep-p);

	free(code);

	string new_key = remote + ":" + k;
	
	key_mutex.lock();
	if (rpt == 0) {
		key = new_key;
		repeat = 0;
		repeat_time = steady_clock::now();
		Publish(NULL, "/devices/" + MQTTConfig.Id + "/controls/Key", new_key + ":0", 0, false);
	}
	if (key == new_key) {
		if (Config.Debug)
			cerr << "remote: " << remote << ", key: " << k << ", repeat: " << rpt << endl;
		last_time = steady_clock::now();
	}
	key_mutex.unlock();
}

void TMQTTLircHandler::key_thread()
{
	while (1) {
		key_mutex.lock();
		auto now = steady_clock::now();
		if (key != "") {
			auto interval = duration_cast<milliseconds>(now - last_time).count() ;
			if (interval > Config.ReleaseTime) {
				last_time = now;
				key = "";
				Publish(NULL, "/devices/" + MQTTConfig.Id + "/controls/Key", "", 0, false);
			}
			interval = duration_cast<milliseconds>(now - repeat_time).count() ;
			if (interval > Config.RepeatTime) {
				repeat_time = now;
				repeat++;
				Publish(NULL, "/devices/" + MQTTConfig.Id + "/controls/Key", key + ":" + to_string(repeat), 0, false);
			}
		}
		key_mutex.unlock();
		this_thread::sleep_for(milliseconds(50));
	}
}

int main(int argc, char **argv)
{
	string config_fname;
	bool debug = false;
	TMQTTLircHandler::TConfig mqtt_config;
	mqtt_config.Host = "localhost";
	mqtt_config.Port = 1883;

	int c;
	//~ int digit_optind = 0;
	//~ int aopt = 0, bopt = 0;
	//~ char *copt = 0, *dopt = 0;
	while ( (c = getopt(argc, argv, "dc:h:p:")) != -1) {
		//~ int this_option_optind = optind ? optind : 1;
		switch (c) {
		case 'd':
			debug = true;
			break;
		case 'c':
			config_fname = optarg;
			break;
		case 'p':
			mqtt_config.Port = stoi(optarg);
			break;
		case 'h':
			mqtt_config.Host = optarg;
			break;
		case '?':
			break;
		default:
			printf ("?? getopt returned character code 0%o ??\n", c);
		}
	}

	try {
		THandlerConfig config;
		if (!config_fname.empty())
			LoadConfig(config_fname, config);

		config.Debug = config.Debug || debug;
		mqtt_config.Id = "wb-lirc";

		shared_ptr<TMQTTLircHandler> mqtt_handler( new TMQTTLircHandler(mqtt_config, config));

		try {
			mqtt_handler->Init();
			int ret = mqtt_handler->loop_start();
			if (ret != 0)
				throw TLircException("Couldn't start mosquitto_loop_start: " + to_string(ret));

			while (1) {
				try {
					mqtt_handler->ConnectToLirc();
					while(1) {
						if (!mqtt_handler->Learn)
							mqtt_handler->NextCode();
					}
				} catch (const TLircException& e) {
					if (config.Debug)
						cerr << "EXCEPTION: " << e.what() << endl;
					this_thread::sleep_for(milliseconds(1000));
				}
			}
		} catch (const TLircException& e) {
			e.fatal();
		}

		mqtt_handler->KeyThread.join();
	} catch (const TLircException& e) {
		e.fatal();
	}

	return 0;
}
