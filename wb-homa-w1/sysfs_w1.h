#pragma once
#include <string>
#include <map>
#include <common/utils.h>

using namespace std;

const string SysfsOnewireDevicesPath = "/sys/bus/w1/devices/";

enum class TOnewireFamilyType
{
    ProgResThermometer = 0x28,
    Unknown = 0x00,
};

class TSysfsOnewireDevice
{
public:
    TSysfsOnewireDevice(const string& device_name);

    inline TOnewireFamilyType GetDeviceFamily() const {return Family;};
    inline const string & GetDeviceId() const {return DeviceId;};

    TMaybe<float> ReadTemperature() const;

    friend bool operator== (const TSysfsOnewireDevice & first, const TSysfsOnewireDevice & second);
private:
    string DeviceName;
    TOnewireFamilyType Family;
    string DeviceId;
    string DeviceDir;


};


class TSysfsOnewireManager
{
public:
    TSysfsOnewireManager()  {};

    void RescanBus();


private:
    // FIXME: once found, device will be kept indefinetely
    // consider some kind of ref-counting smart vectors instead

    map<const string, TSysfsOnewireDevice> Devices;

};


class TOneWireReadErrorException : public std::exception
{
public:
   TOneWireReadErrorException(const std::string& deviceFileName) : Message("1-Wire system : error reading value from ") {Message.append(deviceFileName);}
   virtual ~TOneWireReadErrorException() throw() {}
   virtual const char* what() const throw() {return Message.c_str();}
protected:
   std::string Message;
};

class TOneWireWriteErrorException : public std::exception
{
public:
   TOneWireWriteErrorException(const std::string& deviceFileName) : Message("1-Wire system : error writing value from ") {Message.append(deviceFileName);}
   virtual ~TOneWireWriteErrorException() throw() {}
   virtual const char* what() const throw() {return Message.c_str();}
protected:
   std::string Message;
};
