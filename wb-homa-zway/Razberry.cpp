//#include "stdafx.h"
#include "Razberry.h"

#include <sstream>      // std::stringstream
#include <vector>
#include <ctype.h>

#include "HTTPClient.h"

#include "jsoncpp/json/json.h"
#include "localtime_r.h"

#pragma warning(disable: 4996)

#define round(a) ( int ) ( a + .5 )

#ifdef _DEBUG
	#define DEBUG_ZWAVE_INT
#endif

using namespace std;
static std::string readInputTestFile( const char *path )
{
	FILE *file = fopen( path, "rb" );
	if ( !file )
		return std::string("");
	fseek( file, 0, SEEK_END );
	long size = ftell( file );
	fseek( file, 0, SEEK_SET );
	std::string text;
	char *buffer = new char[size+1];
	buffer[size] = 0;
	if ( fread( buffer, 1, size, file ) == (unsigned long)size )
		text = buffer;
	fclose( file );
	delete[] buffer;
	return text;
}

CRazberry::CRazberry(TMQTTZWay* owner, const int ID, const std::string &ipaddress, const int port, const std::string &username, const std::string &password)
    : Owner(owner)
{
	m_ipaddress=ipaddress;
	m_port=port;
	m_username=username;
	m_password=password;
	m_controllerID=0;
}


CRazberry::~CRazberry(void)
{
}

string CRazberry::MqttEscape(const string& str)
{
    string result = "";
    for (int i = 0; i < str.length(); i++)
        if ((str[i] == '/') || (str[i] == ' '))
            result +="_";
        else 
            result +=str[i];
    return result;
}


const std::string CRazberry::GetControllerURL()
{
	std::stringstream sUrl;
	if (m_username=="")
		sUrl << "http://" << m_ipaddress << ":" << m_port << "/ZWaveAPI/Data/" << m_updateTime;
	else
		sUrl << "http://"  << m_username << ":" << m_password << "@" << m_ipaddress << ":" << m_port << "/ZWaveAPI/Data/" << m_updateTime;
	return sUrl.str();
}

const std::string CRazberry::GetRunURL(const std::string &cmd)
{
	std::stringstream sUrl;
	if (m_username=="")
		sUrl << "http://" << m_ipaddress << ":" << m_port << "/ZWaveAPI/Run/" << cmd;
	else
		sUrl << "http://"  << m_username << ":" << m_password << "@" << m_ipaddress << ":" << m_port << "/ZWaveAPI/Run/" << cmd;
	return sUrl.str();
}

bool CRazberry::GetInitialDevices()
{
	m_updateTime=0;
	std::string sResult;
#ifndef DEBUG_ZWAVE_INT	
	std::string szURL=GetControllerURL();

	bool bret;
	bret=HTTPClient::GET(szURL,sResult);
	if (!bret)
	{
		cerr << "Razberry: Error getting data!";
		return 0;
	}
#else
	sResult=readInputTestFile("test.json");
#endif
	Json::Value root;

	Json::Reader jReader;
	bool ret=jReader.parse(sResult,root);
	if (!ret)
	{
		cerr << "Razberry: Invalid data received!";
		return 0;
	}

	Json::Value jval;
	jval=root["controller"];
	if (jval.empty()==true)
		return 0;
	m_controllerID=jval["data"]["nodeId"]["value"].asInt();

	for (Json::Value::iterator itt=root.begin(); itt!=root.end(); ++itt)
	{
		const std::string kName=itt.key().asString();
		if (kName=="devices")
		{
			parseDevices(*itt);
		}
		else if (kName=="updateTime")
		{
			//cout << "std::string supdateTime=(*itt).asString()\n";
			//std::string supdateTime=(*itt).asString();
			//m_updateTime=(time_t)atol(supdateTime.c_str());
            m_updateTime = (time_t)(*itt).asInt();
		}
	}

	return true;
}

bool CRazberry::GetUpdates()
{
	std::string sResult;
#ifndef	DEBUG_ZWAVE_INT
	std::string szURL=GetControllerURL();
	bool bret;
	bret=HTTPClient::GET(szURL,sResult);
	if (!bret)
	{
		cerr << "Razberry: Error getting update data!";
		return 0;
	}
#else
	sResult=readInputTestFile("update.json");
#endif
	Json::Value root;

	Json::Reader jReader;
	bool ret=jReader.parse(sResult,root);
	if (!ret)
	{
		cerr << "Razberry: Invalid data received!";
		return 0;
	}

	for (Json::Value::iterator itt=root.begin(); itt!=root.end(); ++itt)
	{
		std::string kName=itt.key().asString();
		const Json::Value obj=(*itt);

		if (kName=="updateTime")
		{
			//cout << "std::string supdateTime=obj.asString()\n";
			//std::string supdateTime=obj.asString();
			//m_updateTime=(time_t)atol(supdateTime.c_str());
            m_updateTime = (time_t) obj.asInt();
		}
		else if (kName=="devices")
		{
			parseDevices(obj);
		}
		else
		{
			std::vector<std::string> results;
			StringSplit(kName,".",results);

			if (results.size()>1)
			{
				if (kName.find("lastReceived")==std::string::npos)
					UpdateDevice(kName,obj);
			}
		}
	}

	return true;
}

void CRazberry::InsertControl(_tZWaveDevice device, const string& ctrl_id)
{
    vector<TPublishPair> output_vector;
    string control_id;
    string value = "";
    bool to_subscribe = false;
    switch (device.devType)
    {
        case ZDTYPE_SWITCH_DIMMER:
        case ZDTYPE_SWITCH_NORMAL:
            {
                control_id = to_string(device.instanceID) + "_" + to_string(device.commandClassID);
                value = to_string(device.intvalue);
                if ((device.commandClassID == 37) || (device.commandClassID == 38)) {
                    output_vector.push_back(make_pair("type", "switch"));
                    to_subscribe = true;
                }
                if ((device.commandClassID == 48) || (device.commandClassID == 156)) {
                    output_vector.push_back(make_pair("type", "text"));
                    output_vector.push_back(make_pair("readonly", "1"));
                    control_id = ctrl_id;
                }
                if (device.commandClassID == 64) { 
                    output_vector.push_back(make_pair("type", "multiswitch"));
                    to_subscribe = true;
                }
                break;
            }
        case ZDTYPE_SENSOR_SETPOINT:
            {
                output_vector.push_back(make_pair("type", "temperature"));
                char buff[10];
                sprintf(buff, "$%.2f",device.floatValue);
                value = buff;
                control_id = ctrl_id;
                to_subscribe = true;
                break;
            }
        case ZDTYPE_SENSOR_HUMIDITY:
            {
                value = to_string(device.intvalue);
                output_vector.push_back(make_pair("type", "rel_humidity"));
            }
        case ZDTYPE_SENSOR_POWER:
            {
                if (output_vector.size() < 1) 
                    output_vector.push_back(make_pair("type", "power"));
            }
        case ZDTYPE_SENSOR_LIGHT: {
                if (output_vector.size() < 1) // to ignore of pushing this pair for zdtype_sensor_power
                    output_vector.push_back(make_pair("type", "generic_value"));
            }
        case ZDTYPE_SENSOR_TEMPERATURE:
            {
                if (output_vector.size() < 1) 
                    output_vector.push_back(make_pair("type", "temperature"));
            }
        case ZDTYPE_SENSOR_POWERENERGYMETER:
            {
                if (output_vector.size() < 1) 
                    output_vector.push_back(make_pair("type", "power_consumption"));
            }
        case ZDTYPE_SENSOR_VOLTAGE:
            {
                if (output_vector.size() < 1) 
                    output_vector.push_back(make_pair("type", "voltage"));
            }
        case ZDTYPE_SENSOR_AMPERE:
            {
                if (output_vector.size() < 1) 
                    output_vector.push_back(make_pair("type", "generic_value"));
            }
        case ZDTYPE_SENSOR_PERCENTAGE:
            {
                if (output_vector.size() < 1) 
                    output_vector.push_back(make_pair("type", "generic_value"));
                control_id = to_string(device.instanceID) + "_" + to_string(device.commandClassID) + "_" + to_string(device.scaleID);
                char buff[10];
                sprintf(buff, "$%.2f",device.floatValue);
                if (value == "")
                    value = buff;
                break;
            }
    }
    cout << "control is " << control_id << endl;
    device.ControlId = control_id;        
    InsertDevice(device);
    Owner->PublishControlMeta(device.DeviceId, control_id, output_vector);
    Owner->PublishControl(device.DeviceId, control_id, value);
    if (to_subscribe) {

        Owner->SubscribControl(device.DeviceId, control_id);
    }
         
    
}


void CRazberry::parseDevices(const Json::Value &devroot)
{
	for (Json::Value::iterator itt=devroot.begin(); itt!=devroot.end(); ++itt)
	{
		const std::string devID=itt.key().asString();

		unsigned long nodeID=atol(devID.c_str());
		if ((nodeID==255)||(nodeID==m_controllerID))
			continue; //skip ourself

		const Json::Value node=(*itt);

		_tZWaveDevice _device;
		_device.nodeID=nodeID;
        string device_name = MqttEscape(node["data"]["givenName"]["value"].asString());
        string dev_id = to_string(nodeID) + "_" + device_name; 
        cout << "Here i am " << device_name << " nodID is " << nodeID << endl;
        _device.DeviceId = dev_id;
        Owner->PublishDevice(dev_id, device_name); // publish device /meta/name

		// Device status and battery
		_device.basicType =		node["data"]["basicType"]["value"].asInt();
		_device.genericType =	node["data"]["genericType"]["value"].asInt();
		_device.specificType =	node["data"]["specificType"]["value"].asInt();
		_device.isListening =	node["data"]["isListening"]["value"].asBool();
		_device.sensor250=		node["data"]["sensor250"]["value"].asBool();
		_device.sensor1000=		node["data"]["sensor1000"]["value"].asBool();
		_device.isFLiRS =		!_device.isListening && (_device.sensor250 || _device.sensor1000);
		_device.hasWakeup =		(node["instances"]["0"]["commandClasses"]["132"].empty()==false);
		_device.hasBattery =	(node["instances"]["0"]["commandClasses"]["128"].empty()==false);

		const Json::Value nodeInstances=node["instances"];
		// For all instances
		bool haveMultipleInstance=(nodeInstances.size()>1);
		for (Json::Value::iterator ittInstance=nodeInstances.begin(); ittInstance!=nodeInstances.end(); ++ittInstance)
		{
			_device.commandClassID=0;
			_device.scaleID=-1;

			const std::string sID=ittInstance.key().asString();
			_device.instanceID=atoi(sID.c_str());
			_device.indexID=0;
			if ((_device.instanceID==0)&&(haveMultipleInstance))
				continue;// We skip instance 0 if there are more, since it should be mapped to other instances or their superposition

			const Json::Value instance=(*ittInstance);

			if (_device.hasBattery)
			{
				_device.batValue=instance["commandClasses"]["128"]["data"]["last"]["value"].asInt();
                string control_id = to_string(_device.instanceID) + "_128";;
                vector<TPublishPair> output_vector;
                output_vector.push_back(make_pair("type","generic_value"));// FIXME type for battery ? 
                Owner->PublishControlMeta(dev_id, control_id, output_vector);
                Owner->PublishControl(dev_id, control_id, to_string(_device.batValue));
			}
			// Switches
			// We choose SwitchMultilevel first, if not available, SwhichBinary is chosen
			if (instance["commandClasses"]["38"].empty()==false)
			{
				//COMMAND_CLASS_SWITCH_MULTILEVEL
				_device.commandClassID=38;
				_device.devType= ZDTYPE_SWITCH_DIMMER;
				_device.intvalue=instance["commandClasses"]["38"]["data"]["level"]["value"].asInt();
				InsertControl(_device, "");
                
			}
		    else if (instance["commandClasses"]["37"].empty()==false)
			{
				//COMMAND_CLASS_SWITCH_BINARY
				_device.commandClassID=37;
				_device.devType= ZDTYPE_SWITCH_NORMAL;
				_device.intvalue=instance["commandClasses"]["37"]["data"]["level"]["value"].asInt();
				InsertControl(_device, "");
			}

			// Add Sensor Binary
			if (instance["commandClasses"]["48"].empty()==false)
			{
				//COMMAND_CLASS_SENSOR_BINARY
				_device.commandClassID=48; //(binary switch, for example motion detector(PIR)
				_device.devType= ZDTYPE_SWITCH_NORMAL;
				if (instance["commandClasses"]["48"]["data"]["level"].empty()==false)
				{
					_device.intvalue=instance["commandClasses"]["48"]["data"]["level"]["value"].asInt();
                    cout << "AAAAAAAAAAAAA VALUE IS " << _device.intvalue;
                    string control_id = to_string(_device.instanceID) + "_" +  to_string(_device.commandClassID);
                    InsertControl(_device, control_id);
				}
				else
				{
					const Json::Value inVal=instance["commandClasses"]["48"]["data"];
					for (Json::Value::iterator itt2=inVal.begin(); itt2!=inVal.end(); ++itt2)
					{
						const std::string sKey=itt2.key().asString();
						if (!isInt(sKey))
							continue; //not a scale
						if ((*itt2)["level"].empty()==false)
						{
							_device.sensorID=atoi(sKey.c_str());
							_device.indexID=0;
							std::string vstring=(*itt2)["level"]["value"].asString();
							if (vstring=="true")
								_device.intvalue=1;//FIXME  find out why there was 255
							else if (vstring=="false")
								_device.intvalue=0;
							else
								_device.intvalue=atoi(vstring.c_str());
                        
                        string control_id = to_string(_device.instanceID) + "_" + to_string(_device.commandClassID) + "_" + to_string(_device.sensorID);
                        InsertControl(_device, control_id);
						}
					}
				}
			}
			// Add Sensor Multilevel
			if (instance["commandClasses"]["49"].empty()==false)
			{
				//COMMAND_CLASS_SENSOR_MULTILEVEL
				_device.commandClassID=49;
				_device.scaleMultiply=1;
				const Json::Value inVal=instance["commandClasses"]["49"]["data"];
				for (Json::Value::iterator itt2=inVal.begin(); itt2!=inVal.end(); ++itt2)
				{
					const std::string sKey=itt2.key().asString();
					if (!isInt(sKey))
						continue; //not a scale
					_device.scaleID=atoi(sKey.c_str());
					std::string sensorTypeString = (*itt2)["sensorTypeString"]["value"].asString();
					if (sensorTypeString=="Power")
					{
						_device.floatValue=(*itt2)["val"]["value"].asFloat();
						if (_device.scaleID == 4)
						{
							_device.commandClassID=49;
							_device.devType = ZDTYPE_SENSOR_POWER;
                            InsertControl(_device, "");
						}
					}
					else if (sensorTypeString=="Temperature")
					{
						_device.floatValue=(*itt2)["val"]["value"].asFloat();
						_device.commandClassID=49;
						_device.devType = ZDTYPE_SENSOR_TEMPERATURE;
                        InsertControl(_device, "");
					}
					else if (sensorTypeString=="Humidity")
					{
						_device.intvalue=(*itt2)["val"]["value"].asInt();
						_device.commandClassID=49;
						_device.devType = ZDTYPE_SENSOR_HUMIDITY;
                        InsertControl(_device, "");
					}
					else if (sensorTypeString=="Luminiscence")
					{
						_device.floatValue=(*itt2)["val"]["value"].asFloat();
						_device.commandClassID=49;
						_device.devType = ZDTYPE_SENSOR_LIGHT;
                        InsertControl(_device, "");
					}
				}
				//InsertControl(_device);
			}

			// Meters which are supposed to be sensors (measurable)
			if (instance["commandClasses"]["50"].empty()==false)
			{
				//COMMAND_CLASS_METER
				const Json::Value inVal=instance["commandClasses"]["50"]["data"];
				for (Json::Value::iterator itt2=inVal.begin(); itt2!=inVal.end(); ++itt2)
				{
					const std::string sKey=itt2.key().asString();
					if (!isInt(sKey))
						continue; //not a scale
					_device.scaleID=atoi(sKey.c_str());
					int sensorType=(*itt2)["sensorType"]["value"].asInt();
					_device.floatValue=(*itt2)["val"]["value"].asFloat();
					std::string scaleString = (*itt2)["scaleString"]["value"].asString();
					if ((_device.scaleID == 0 || _device.scaleID == 2 || _device.scaleID == 4 || _device.scaleID == 5 || _device.scaleID == 6) && (sensorType == 1))
					{
						_device.commandClassID=50;
						_device.scaleMultiply=1;
						if (scaleString=="kWh")
						{
							_device.scaleMultiply=1000;
							_device.devType = ZDTYPE_SENSOR_POWERENERGYMETER;
						}
						else if (scaleString=="W")
						{
							_device.devType = ZDTYPE_SENSOR_POWER;
						}
						else if (scaleString=="V")
						{
							_device.devType = ZDTYPE_SENSOR_VOLTAGE;
						}
						else if (scaleString=="A")
						{
							_device.devType = ZDTYPE_SENSOR_AMPERE;
						}
						else if (scaleString=="Power Factor")
						{
							_device.devType = ZDTYPE_SENSOR_PERCENTAGE;
						}
						else
						{
							cerr << "Razberry: Device Scale not handled at the moment, please report (nodeID:" << _device.nodeID << " instanceID:" << _device.instanceID << " Scale:" << scaleString.c_str() << ")\n";
							continue;
						}

						InsertControl(_device, "");
					}
				}
			}

			// Meters (true meter values)
			if (instance["commandClasses"]["50"].empty()==false)
			{
				//COMMAND_CLASS_METER
				const Json::Value inVal=instance["commandClasses"]["50"]["data"];
				for (Json::Value::iterator itt2=inVal.begin(); itt2!=inVal.end(); ++itt2)
				{
					const std::string sKey=itt2.key().asString();
					if (!isInt(sKey))
						continue; //not a scale
					_device.scaleID=atoi(sKey.c_str());
					int sensorType=(*itt2)["sensorType"]["value"].asInt();
					_device.floatValue=(*itt2)["val"]["value"].asFloat();
					std::string scaleString = (*itt2)["scaleString"]["value"].asString();
					if ((_device.scaleID == 0 || _device.scaleID == 2 || _device.scaleID == 4 || _device.scaleID == 5 || _device.scaleID == 6) && (sensorType == 1))
						continue; // we don't want to have measurable here (W, V, A, PowerFactor)
					_device.commandClassID=50;
					_device.scaleMultiply=1;
					if (scaleString=="kWh")
					{
						_device.scaleMultiply=1000;
						_device.devType = ZDTYPE_SENSOR_POWERENERGYMETER;
					}
					else if (scaleString=="W")
					{
						_device.devType = ZDTYPE_SENSOR_POWER;
					}
					else if (scaleString=="V")
					{
						_device.devType = ZDTYPE_SENSOR_VOLTAGE;
					}
					else if (scaleString=="A")
					{
						_device.devType = ZDTYPE_SENSOR_AMPERE;
					}
					else if (scaleString=="Power Factor")
					{
						_device.devType = ZDTYPE_SENSOR_PERCENTAGE;
					}
					else
					{
						cerr << "Razberry: Device Scale not handled at the moment, please report (nodeID:" << _device.nodeID << " instanceID:" << _device.instanceID << " Scale:" << scaleString.c_str() << ")\n";
						continue;
					}
					InsertControl(_device, "");
				}
			}
			if (instance["commandClasses"]["64"].empty()==false)
			{
				//COMMAND_CLASS_THERMOSTAT_MODE
				int iValue=instance["commandClasses"]["64"]["data"]["mode"]["value"].asInt();
				/*if (iValue==0)
					_device.intvalue=0;
				else
					_device.intvalue=255;*/
				_device.commandClassID=64;
				_device.devType = ZDTYPE_SWITCH_NORMAL;
				InsertControl(_device, "");
			}
			if (instance["commandClasses"]["67"].empty()==false)
			{
				//COMMAND_CLASS_THERMOSTAT_SETPOINT
				const Json::Value inVal=instance["commandClasses"]["67"]["data"];
				for (Json::Value::iterator itt2=inVal.begin(); itt2!=inVal.end(); ++itt2)
				{
					const std::string sKey=itt2.key().asString();
					if (!isInt(sKey))
						continue; //not a scale
					_device.floatValue = (*itt2)["val"]["value"].asFloat();
                    string control_id = to_string(_device.instanceID) + "_" + to_string(_device.commandClassID) + "_" + "_" + sKey;
                    _device.sensorID = atoi(sKey.c_str());
					_device.commandClassID=67;
					_device.scaleMultiply=1;
					_device.devType = ZDTYPE_SENSOR_SETPOINT;
					InsertControl(_device, control_id);
				}
			}
			else if (instance["commandClasses"]["156"].empty()==false)
			{
				//COMMAND_CLASS_SENSOR_ALARM
				const Json::Value inVal=instance["commandClasses"]["156"]["data"];
				for (Json::Value::iterator itt2=inVal.begin(); itt2!=inVal.end(); ++itt2)
				{
					const std::string sKey=itt2.key().asString();
					if (!isInt(sKey))
						continue; //not a scale
                    _device.sensorID = atoi(sKey.c_str());
					_device.intvalue=(*itt2)["sensorState"]["value"].asInt();// sensorSate or sensorState ??
                    string control_id = to_string(_device.instanceID) + "_" + to_string(_device.commandClassID) + "_" + sKey;
					_device.commandClassID=156;
					_device.devType = ZDTYPE_SWITCH_NORMAL;
					InsertControl(_device, control_id);
                
				}
			}
			//COMMAND_CLASS_CLIMATE_CONTROL_SCHEDULE 70
		}
	}
}

void CRazberry::UpdateDevice(const std::string &path, const Json::Value &obj)
{
	_tZWaveDevice *pDevice=NULL;

	if (
		(path.find("srcId")!=std::string::npos)||
		(path.find("sensorTime")!=std::string::npos)
		)
		return;

	if (path.find("instances.0.commandClasses.128.data.last")!=std::string::npos)
	{
		//COMMAND_CLASS_BATTERY
		if (obj["value"].empty()==false)
		{
			int batValue=obj["value"].asInt();
			std::vector<std::string> results;
			StringSplit(path,".",results);
			int devID=atoi(results[1].c_str());
			UpdateDeviceBatteryStatus(devID,batValue);
            string control_id = to_string(devID) + "_128";
            _tZWaveDevice *temp = FindDeviceByNodeId(devID);
            Owner->PublishControl(temp->DeviceId, control_id, to_string(batValue));
		}
	}
	else if (path.find("instances.0.commandClasses.49.data.")!=std::string::npos)
	{
		//COMMAND_CLASS_SENSOR_MULTILEVEL
		//Possible fix for Everspring ST814. maybe others, my multi sensor is coming soon to find out!
		std::vector<std::string> results;
		StringSplit(path,".",results);
		//Find device by data id
		if (results.size()==8)
		{
			int cmdID=atoi(results[5].c_str());
			if (cmdID==49)
			{
				int devID=atoi(results[1].c_str());
				int scaleID=atoi(results[7].c_str());
				pDevice=FindDeviceByScale(devID,scaleID);
			}
		}
	}
	else if (path.find("instances.0.commandClasses.48.data.")!=std::string::npos)
	{
		//COMMAND_CLASS_SENSOR_BINARY
		//Possible fix for door sensors reporting on another instance number
		std::vector<std::string> results;
		StringSplit(path,".",results);
		//Find device by data id
		if (results.size()==8)
		{
			int cmdID=atoi(results[5].c_str());
			if (cmdID==48)
			{
				int devID = atoi(results[1].c_str());
				int scaleID = atoi(results[7].c_str());
				pDevice=FindDeviceByScale(devID, scaleID);
			}
		}
	}
	else if (path.find("instances.0.commandClasses.64.data.")!=std::string::npos)
	{
		//COMMAND_CLASS_THERMOSTAT_MODE
		std::vector<std::string> results;
		StringSplit(path,".",results);
		//Find device by data id
		if (results.size()==8)
		{
			int cmdID=atoi(results[5].c_str());
			if (cmdID==64)
			{
				int devID=atoi(results[1].c_str());
				int instanceID=atoi(results[3].c_str());
				int indexID=0;
				pDevice=FindDevice(devID,instanceID,indexID, cmdID, ZDTYPE_SWITCH_NORMAL);
			}
		}
	}
	/*else if (path.find("commandClasses.43.data.currentScene")!=std::string::npos)
	{
		//COMMAND_CLASS_SCENE_ACTIVATION
		std::vector<std::string> results;
		StringSplit(path,".",results);
		//Find device by data id
		if (results.size()==8)
		{
			int cmdID=atoi(results[5].c_str());
			if (cmdID==43)
			{
				int iScene=obj["value"].asInt();
				int devID=(iScene<<8)+atoi(results[1].c_str());
				int instanceID=atoi(results[3].c_str());
				int indexID=0;
				if (instanceID==0)
				{
					//only allow instance 0 for now
					pDevice=FindDevice(devID,instanceID, indexID, cmdID, ZDTYPE_SWITCH_NORMAL);
					if (pDevice==NULL)
					{
						//Add new switch device
						_tZWaveDevice _device;
						_device.nodeID=devID;
						_device.instanceID=instanceID;
						_device.indexID=indexID;

						_device.basicType =		1;
						_device.genericType =	1;
						_device.specificType =	1;
						_device.isListening =	false;
						_device.sensor250=		false;
						_device.sensor1000=		false;
						_device.isFLiRS =		!_device.isListening && (_device.sensor250 || _device.sensor1000);
						_device.hasWakeup =		false;
						_device.hasBattery =	false;
						_device.scaleID=-1;

						_device.commandClassID=cmdID;
						_device.devType= ZDTYPE_SWITCH_NORMAL;
						_device.intvalue=255;
                        string control_id = to_string(devID) + "_" + to_string(
						InsertControl(_device);
						pDevice=FindDevice(devID,instanceID,indexID, cmdID, ZDTYPE_SWITCH_NORMAL);
					}
				}
			}
		}
	}*/
	if (pDevice==NULL)
	{
		std::map<std::string,_tZWaveDevice>::iterator itt;
		for (itt=m_devices.begin(); itt!=m_devices.end(); ++itt)
		{
			std::string::size_type loc = path.find(itt->second.string_id,0);
			if (loc!=std::string::npos)
			{
				pDevice=&itt->second;
				break;
			}
		}
	}

	if (pDevice==NULL)
	{
		//Special Case for Controller received light commands
		size_t iPos= path.find(".data.level");
		if (iPos==std::string::npos)
		{
			return;
		}
		std::string tmpStr;
		//create this device
		_tZWaveDevice _device;

		//Get device node ID
		size_t pPos=path.find(".");
		if (pPos==std::string::npos)
			return;
		tmpStr=path.substr(pPos+1);
		pPos=tmpStr.find(".");
		if (pPos==std::string::npos)
			return;
		std::string sNodeID=tmpStr.substr(0,pPos);
		_device.nodeID=atoi(sNodeID.c_str());

		//Find instance ID
		pPos=path.find("instances.");
		if (pPos==std::string::npos)
			return;
		tmpStr=path.substr(pPos+sizeof("instances.")-1);
		pPos=tmpStr.find(".");
		if (pPos==std::string::npos)
			return;
		std::string sInstanceID=tmpStr.substr(0,pPos);
		_device.instanceID=atoi(sInstanceID.c_str());
		_device.indexID=0;
		pPos=path.find("commandClasses.");
		if (pPos==std::string::npos)
			return;
		tmpStr=path.substr(pPos+sizeof("commandClasses.")-1);
		pPos=tmpStr.find(".");
		if (pPos==std::string::npos)
			return;
		std::string sClassID=tmpStr.substr(0,pPos);
		

		// Device status and battery
		_device.basicType =		1;
		_device.genericType =	1;
		_device.specificType =	1;
		_device.isListening =	false;
		_device.sensor250=		false;
		_device.sensor1000=		false;
		_device.isFLiRS =		!_device.isListening && (_device.sensor250 || _device.sensor1000);
		_device.hasWakeup =		false;
		_device.hasBattery =	false;
		_device.scaleID=-1;

		_device.commandClassID=atoi(sClassID.c_str());
		_device.devType= ZDTYPE_SWITCH_NORMAL;
		std::string vstring=obj["value"].asString();
		if (vstring=="true")
			_device.intvalue=255;
		else if (vstring=="false")
			_device.intvalue=0;
		else
			_device.intvalue=obj["value"].asInt();

		bool bFoundInstance=false;
		int oldinstance=_device.instanceID;
		for (int iInst=0; iInst<7; iInst++)
		{
			_device.instanceID=iInst;
			std:: string devicestring_id=GenerateDeviceStringID(&_device);
			std::map<std::string,_tZWaveDevice>::iterator iDevice=m_devices.find(devicestring_id);
			if (iDevice!=m_devices.end())
			{
				bFoundInstance=true;
				break;
			}
		}
		if (!bFoundInstance)
			_device.instanceID=oldinstance;
        
		InsertControl(_device,"");

		//find device again
		std:: string devicestring_id=GenerateDeviceStringID(&_device);
		std::map<std::string,_tZWaveDevice>::iterator iDevice=m_devices.find(devicestring_id);
		if (iDevice==m_devices.end())
			return; //uhuh?
		pDevice=&iDevice->second;
	}

	time_t atime=mytime(NULL);
//	if (atime-pDevice->lastreceived<2)
	//	return; //to soon
#ifdef _DEBUG
	cerr << "Razberry: Update device: " <<pDevice->string_id.c_str() << endl;
#endif
    string value = "";
	switch (pDevice->devType)
	{
	case ZDTYPE_SWITCH_NORMAL:
	case ZDTYPE_SWITCH_DIMMER:
		{
			//switch
			int intValue = 0;
			if (pDevice->commandClassID==64) //Thermostat Mode
			{
				int iValue=obj["value"].asInt();
				if (iValue==0)
					intValue = 0;
				else
					intValue = 255;
			}
			else if (pDevice->commandClassID==43) //Switch Scene
			{
				intValue = 255;
			}
			else
			{
				std::string vstring="";
				if (obj["value"].empty()==false)
					vstring=obj["value"].asString();
				else if (obj["level"].empty()==false)
				{
					if (obj["level"]["value"].empty()==false)
						vstring=obj["level"]["value"].asString();
				}

				if (vstring=="true")
					intValue = 255;
				else if (vstring=="false")
					intValue = 0;
				else
					intValue = atoi(vstring.c_str());
			}
			if (pDevice->intvalue == intValue)
			{
				//Don't send same value twice
				pDevice->lastreceived = atime;
				pDevice->sequence_number += 1;
				if (pDevice->sequence_number == 0)
					pDevice->sequence_number = 1;
				return;
			}
			pDevice->intvalue = intValue;
            value = to_string(intValue);
		}
		break;
	case ZDTYPE_SENSOR_POWER:
        {
		//meters
		pDevice->floatValue=obj["val"]["value"].asFloat()*pDevice->scaleMultiply;
        char buff[10];
        sprintf(buff, "$%.2f",pDevice->floatValue);
        value = buff;
		break;
        }
	case ZDTYPE_SENSOR_VOLTAGE:
        {
		//Voltage
		pDevice->floatValue=obj["val"]["value"].asFloat()*pDevice->scaleMultiply;
        char buff[10];
        sprintf(buff, "$%.2f",pDevice->floatValue);
        value = buff;
		break;
        }
	case ZDTYPE_SENSOR_AMPERE:
        {
		//Ampere
		pDevice->floatValue=obj["val"]["value"].asFloat()*pDevice->scaleMultiply;
        char buff[10];
        sprintf(buff, "$%.2f",pDevice->floatValue);
        value = buff;
		break;
        }
	case ZDTYPE_SENSOR_PERCENTAGE:
        {
		//Ampere
		pDevice->floatValue=obj["val"]["value"].asFloat()*pDevice->scaleMultiply;
        char buff[10];
        sprintf(buff, "$%.2f",pDevice->floatValue);
        value = buff;
		break;
        }
	case ZDTYPE_SENSOR_POWERENERGYMETER:
        {
		pDevice->floatValue=obj["val"]["value"].asFloat()*pDevice->scaleMultiply;
        char buff[10];
        sprintf(buff, "$%.2f",pDevice->floatValue);
        value = buff;
		break;
        }
	case ZDTYPE_SENSOR_TEMPERATURE:
        {
		//meters
		pDevice->floatValue=obj["val"]["value"].asFloat();
        char buff[10];
        sprintf(buff, "$%.2f",pDevice->floatValue);
        value = buff;
		break;
        }
	case ZDTYPE_SENSOR_HUMIDITY:
        {
		//switch
		pDevice->intvalue=obj["val"]["value"].asInt();
        value = to_string(pDevice->intvalue);
		break;
        }
	case ZDTYPE_SENSOR_LIGHT:
        {
		//switch
		pDevice->floatValue=obj["val"]["value"].asFloat();
        char buff[10];
        sprintf(buff, "$%.2f",pDevice->floatValue);
        value = buff;
		break;
        }
	case ZDTYPE_SENSOR_SETPOINT:
        {
		//meters
		if (obj["val"]["value"].empty()==false)
		{
			pDevice->floatValue=obj["val"]["value"].asFloat();
		}
		else if (obj["value"].empty()==false)
		{
			pDevice->floatValue=obj["value"].asFloat();
		}
        char buff[10];
        sprintf(buff, "$%.2f",pDevice->floatValue);
        value = buff;
		break;
        }
	}
    Owner->PublishControl(pDevice->DeviceId, pDevice->ControlId, value);
	pDevice->lastreceived=atime;
	pDevice->sequence_number+=1;
	if (pDevice->sequence_number==0)
		pDevice->sequence_number=1;
}

ZWaveBase::_tZWaveDevice* CRazberry::FindDeviceByControl(const string& dev_id, const string& control_id)
{
	std::map<std::string,_tZWaveDevice>::iterator itt;
	for (itt=m_devices.begin(); itt!=m_devices.end(); ++itt)
	{
		if ((itt->second.ControlId == control_id) && (itt->second.DeviceId == dev_id))
			return &itt->second;
	}
	return nullptr;
}


void CRazberry::SendCommand(const string& dev_id, const string& control_id, const string& payload)
{
    const ZWaveBase::_tZWaveDevice* device = FindDeviceByControl(dev_id, control_id);
 
    switch (device->devType)
    {
        case ZDTYPE_SWITCH_DIMMER:
        case ZDTYPE_SWITCH_NORMAL:
            {
                if ((device->commandClassID == 37) || (device->commandClassID == 38) || (device->commandClassID == 64)) {
                    SwitchOn(*device, payload);
                }
                break;
            }
        case ZDTYPE_SENSOR_SETPOINT:
            {
                SetThermostatSetPoint(*device, payload);
                break;
            }
    }
}

ZWaveBase::_tZWaveDevice* CRazberry::FindDeviceByNodeId(const int node_id)
{
	std::map<std::string,_tZWaveDevice>::iterator itt;
	for (itt=m_devices.begin(); itt!=m_devices.end(); ++itt)
	{
		if (itt->second.nodeID == node_id)
			return &itt->second;
	}
	return NULL;
}

ZWaveBase::_tZWaveDevice* CRazberry::FindDeviceByScale(const int nodeID, const int scaleID)
{
	std::map<std::string,_tZWaveDevice>::iterator itt;
	for (itt=m_devices.begin(); itt!=m_devices.end(); ++itt)
	{
		if (
			(itt->second.nodeID==nodeID)&&
			(itt->second.scaleID==scaleID)
			)
			return &itt->second;
	}
	return NULL;
}


void CRazberry::SwitchOn(const _tZWaveDevice& device, const string& value)
{
	//Send command
	std::stringstream sstr;
	int iValue=atoi(value.c_str());
	if ((device.commandClassID == 64)&&(iValue != 0))
		iValue=1;
    cout << "device is " << device.nodeID << endl;
	sstr << "devices[" << device.nodeID << "].instances[" << device.instanceID << "].commandClasses[" << device.commandClassID << "].Set(" << iValue << ")";
    cout << "runcmd is " << sstr.str() << endl;
	RunCMD(sstr.str());
}


void CRazberry::SetThermostatSetPoint(const _tZWaveDevice& device, const string& value)
{
	std::stringstream sstr;
	sstr << "devices[" << device.nodeID << "].instances[" << device.instanceID << "].commandClasses[" << device.commandClassID << "].Set(" << device.sensorID << "," << value << ",null)";
	RunCMD(sstr.str());
}

void CRazberry::RunCMD(const std::string &cmd)
{
	std::string szURL=GetRunURL(cmd);
	bool bret;
	std::string sResult;
	bret=HTTPClient::GET(szURL,sResult);
	if (!bret)
	{
		cerr << "Razberry: Error sending command to controller!";
	}
}

