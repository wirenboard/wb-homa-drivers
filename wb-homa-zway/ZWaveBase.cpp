#include "ZWaveBase.h"

#include <sstream>      // std::stringstream
#include <vector>
#include <ctype.h>

#include "Helper.h"

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

bool ZWaveBase::StartHardware()
{
	m_bInitState=true;
	m_stoprequested=false;
	m_updateTime=0;
	m_LastIncludedNode=0;
	m_bControllerCommandInProgress=false;
	//m_bIsStarted=true;

    /*//Start worker thread
	m_thread = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&ZWaveBase::Do_Work, this)));
	return (m_thread!=NULL);*/
}

bool ZWaveBase::StopHardware()
{
	if (m_thread!=NULL)
	{
		assert(m_thread);
		m_stoprequested = true;
		if (m_thread!=NULL)
			m_thread->join();
	}
	//m_bIsStarted=false;
	return true;
}

/*void ZWaveBase::Do_Work()
{
#ifdef WIN32
	//prevent OpenZWave locale from taking over
	_configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
#endif
	while (!m_stoprequested)
	{
		sleep_milliseconds(500);

		time_t atime = mytime(NULL);
		struct tm ltime;
		localtime_r(&atime, &ltime);


		if (ltime.tm_sec % 12 == 0) {
			mytime(&m_LastHeartbeat);
		}

		if (m_stoprequested)
			return;
		if (m_bInitState)
		{
			if (GetInitialDevices())
			{
				m_bInitState=false;
				sOnConnected(this);
			}
		}
		else
		{
			GetUpdates();
			if (m_bControllerCommandInProgress==true)
			{
				time_t atime=mytime(NULL);
				time_t tdiff=atime-m_ControllerCommandStartTime;
				if (tdiff>=CONTROLLER_COMMAND_TIMEOUT)
				{
					_log.Log(LOG_ERROR,"ZWave: Stopping Controller command (Timeout!)");
					CancelControllerCommand();
				}
			}
		}
	}
}*/


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

	//SendSwitchIfNotExists(&device);
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

/*void ZWaveBase::SendSwitchIfNotExists(const _tZWaveDevice *pDevice)
{
	if (
		(pDevice->devType!=ZDTYPE_SWITCH_NORMAL)&&
		(pDevice->devType != ZDTYPE_SWITCH_DIMMER) &&
		(pDevice->devType != ZDTYPE_SWITCH_FGRGBWM441)
		)
		return; //only for switches

	if (pDevice->devType == ZDTYPE_SWITCH_FGRGBWM441)
	{
		unsigned char ID1 = 0;
		unsigned char ID2 = 0;
		unsigned char ID3 = 0;
		unsigned char ID4 = 0;

		//make device ID
		ID1 = 0;
		ID2 = (unsigned char)((pDevice->nodeID & 0xFF00) >> 8);
		ID3 = (unsigned char)pDevice->nodeID & 0xFF;
		ID4 = pDevice->instanceID;

		//To fix all problems it should be
		//ID1 = (unsigned char)((pDevice->nodeID & 0xFF00) >> 8);
		//ID2 = (unsigned char)pDevice->nodeID & 0xFF;
		//ID3 = pDevice->instanceID;
		//ID4 = pDevice->indexID;
		//but current users gets new devices in this case

		unsigned long lID = (ID1 << 24) + (ID2 << 16) + (ID3 << 8) + ID4;

		char szID[10];
		sprintf(szID, "%08x", (unsigned int)lID);
		std::string ID = szID;
		unsigned char unitcode = 1;

		//Check if we already exist
		std::stringstream szQuery;
		std::vector<std::vector<std::string> > result;
		szQuery << "SELECT ID FROM DeviceStatus WHERE (HardwareID==" << m_HwdID << ") AND (Unit==" << int(unitcode) << ") AND (Type==" << pTypeLimitlessLights << ") AND (SubType==" << sTypeLimitlessRGBW << ") AND (DeviceID=='" << ID << "')";
		result = m_sql.query(szQuery.str());
		if (result.size() > 0)
			return; //Already in the system

		//Send as LimitlessLight
		_tLimitlessLights lcmd;
		lcmd.id = lID;
		lcmd.command = Limitless_LedOff;
		lcmd.value = 0;
		sDecodeRXMessage(this, (const unsigned char *)&lcmd);

		//Set Name
		szQuery.clear();
		szQuery.str("");
		szQuery << "UPDATE DeviceStatus SET Name='" << pDevice->label << "' WHERE (HardwareID==" << m_HwdID << ") AND (DeviceID=='" << ID << "')";
		result = m_sql.query(szQuery.str());
	}
	else
	{
		unsigned char ID1 = 0;
		unsigned char ID2 = 0;
		unsigned char ID3 = 0;
		unsigned char ID4 = 0;

		//make device ID
		ID1 = 0;
		ID2 = (unsigned char)((pDevice->nodeID & 0xFF00) >> 8);
		ID3 = (unsigned char)pDevice->nodeID & 0xFF;
		ID4 = pDevice->instanceID;

		//To fix all problems it should be
		//ID1 = (unsigned char)((pDevice->nodeID & 0xFF00) >> 8);
		//ID2 = (unsigned char)pDevice->nodeID & 0xFF;
		//ID3 = pDevice->instanceID;
		//ID4 = pDevice->indexID;
		//but current users gets new devices in this case

		char szID[10];
		sprintf(szID, "%X%02X%02X%02X", ID1, ID2, ID3, ID4);
		std::string ID = szID;
		unsigned char unitcode = 1;

		//Check if we already exist
		std::stringstream szQuery;
		std::vector<std::vector<std::string> > result;
		szQuery << "SELECT ID FROM DeviceStatus WHERE (HardwareID==" << m_HwdID << ") AND (Unit==" << int(unitcode) << ") AND (Type==" << pTypeLighting2 << ") AND (SubType==" << sTypeAC << ") AND (DeviceID=='" << ID << "')";
		result = m_sql.query(szQuery.str());
		if (result.size() > 0)
			return; //Already in the system

		//Send as Lighting 2

		tRBUF lcmd;
		memset(&lcmd, 0, sizeof(RBUF));
		lcmd.LIGHTING2.packetlength = sizeof(lcmd.LIGHTING2) - 1;
		lcmd.LIGHTING2.packettype = pTypeLighting2;
		lcmd.LIGHTING2.subtype = sTypeAC;
		lcmd.LIGHTING2.seqnbr = pDevice->sequence_number;
		lcmd.LIGHTING2.id1 = ID1;
		lcmd.LIGHTING2.id2 = ID2;
		lcmd.LIGHTING2.id3 = ID3;
		lcmd.LIGHTING2.id4 = ID4;
		lcmd.LIGHTING2.unitcode = unitcode;
		int level = 15;
		if (pDevice->devType == ZDTYPE_SWITCH_NORMAL)
		{
			//simple on/off device
			if (pDevice->intvalue == 0)
			{
				level = 0;
				lcmd.LIGHTING2.cmnd = light2_sOff;
			}
			else
			{
				level = 15;
				lcmd.LIGHTING2.cmnd = light2_sOn;
			}
		}
		else
		{
			//dimmer able device
			if (pDevice->intvalue == 0)
				level = 0;
			if (pDevice->intvalue == 255)
				level = 15;
			else
			{
				float flevel = (15.0f / 100.0f)*float(pDevice->intvalue);
				level = round(flevel);
				if (level > 15)
					level = 15;
			}
			if (level == 0)
				lcmd.LIGHTING2.cmnd = light2_sOff;
			else if (level == 15)
				lcmd.LIGHTING2.cmnd = light2_sOn;
			else
				lcmd.LIGHTING2.cmnd = light2_sSetLevel;
		}
		lcmd.LIGHTING2.level = level;
		lcmd.LIGHTING2.filler = 0;
		lcmd.LIGHTING2.rssi = 12;

		sDecodeRXMessage(this, (const unsigned char *)&lcmd.LIGHTING2);

		//Set Name
		szQuery.clear();
		szQuery.str("");
		szQuery << "UPDATE DeviceStatus SET Name='" << pDevice->label << "' WHERE (HardwareID==" << m_HwdID << ") AND (DeviceID=='" << ID << "')";
		result = m_sql.query(szQuery.str());
	}
}*/

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

