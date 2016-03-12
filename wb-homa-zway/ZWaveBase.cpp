#include "ZWaveBase.h"

#include <sstream>      // std::stringstream
#include <vector>
#include <ctype.h>


#include "jsoncpp/json/json.h"
#include <assert.h>

#define CONTROLLER_COMMAND_TIMEOUT 20

#pragma warning(disable: 4996)

#define round(a) ( int ) ( a + .5 )

ZWaveBase::ZWaveBase()
{
	m_LastIncludedNode=0;
	m_bControllerCommandInProgress=false;
	m_updateTime=0;
}


ZWaveBase::~ZWaveBase(void)
{
}





std::string ZWaveBase::GenerateDeviceStringID(const _tZWaveDevice *pDevice)
{
	std::stringstream sstr;
	sstr << pDevice->nodeID << ".instances." << pDevice->instanceID << ".commandClasses." << pDevice->commandClassID << ".data";
	if (pDevice->scaleID!=-1)
	{
		sstr << "." << pDevice->scaleID;
	}
	return sstr.str();
}

void ZWaveBase::InsertDevice(_tZWaveDevice device)
{
	device.string_id=GenerateDeviceStringID(&device);

	bool bNewDevice=(m_devices.find(device.string_id)==m_devices.end());
	
	//device.lastreceived=time(NULL); FIX ME TIME 
#ifdef _DEBUG
	if (bNewDevice)
	{
		//_log.Log(LOG_NORM, "New device: %s", device.string_id.c_str());
	}
	else
	{
		//_log.Log(LOG_NORM, "Update device: %s", device.string_id.c_str());
	}
#endif
	//insert or update device in internal record
	device.sequence_number=1;
	m_devices[device.string_id]=device;

}

void ZWaveBase::UpdateDeviceBatteryStatus(const int nodeID, const int value)
{
	std::map<std::string,_tZWaveDevice>::iterator itt;
	for (itt=m_devices.begin(); itt!=m_devices.end(); ++itt)
	{
		if (itt->second.nodeID==nodeID)
		{
			itt->second.batValue=value;
			itt->second.hasBattery=true;//we got an update, so it should have a battery then...
		}
	}
}


unsigned char ZWaveBase::Convert_Battery_To_PercInt(const unsigned char level)
{
	int ret=(level/10)-1;
	if (ret<0)
		ret = 0;
	return (unsigned char)ret;
}


ZWaveBase::_tZWaveDevice* ZWaveBase::FindDevice(const int nodeID, const int instanceID, const int indexID, const _eZWaveDeviceType devType)
{
	std::map<std::string,_tZWaveDevice>::iterator itt;
	for (itt=m_devices.begin(); itt!=m_devices.end(); ++itt)
	{
		if (
			(itt->second.nodeID==nodeID)&&
			((itt->second.instanceID==instanceID)||(instanceID==-1))&&
			(itt->second.devType==devType)
			)
			return &itt->second;
	}
	return NULL;
}

ZWaveBase::_tZWaveDevice* ZWaveBase::FindDevice(const int nodeID, const int instanceID, const int indexID, const int CommandClassID,  const _eZWaveDeviceType devType)
{
	std::map<std::string,_tZWaveDevice>::iterator itt;
	for (itt=m_devices.begin(); itt!=m_devices.end(); ++itt)
	{
		if (
			(itt->second.nodeID==nodeID)&&
			((itt->second.instanceID==instanceID)||(instanceID==-1))&&
			(itt->second.commandClassID==CommandClassID)&&
			(itt->second.devType==devType)
			)
			return &itt->second;
	}
	return NULL;
}

