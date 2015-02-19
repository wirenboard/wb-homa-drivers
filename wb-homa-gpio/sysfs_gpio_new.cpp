#include"sysfs_gpio.h"
#include<iostream>
#include<memory>

using namespace std;

TSysfsGpioNew::TSysfsGpioNew(int gpio, bool inverted, string type, int multiplier)
    : TSysfsGpio( gpio, inverted)
    , Type(type)
    , Multiplier(multiplier)
    , Total(0)
{   
    if (Type == "watt_meter" ){
        InitWattMeter();
    }

}

TSysfsGpioNew::TSysfsGpioNew( TSysfsGpioNew&& tmp)
    : TSysfsGpio(std::move(tmp))
    , Type(tmp.Type)
    , Multiplier(tmp.Multiplier)
    , Total(tmp.Total)
{ 
    cout << "MOVE constructor is here !" << endl;
}

void TSysfsGpioNew::InitWattMeter(){
    SetFront("rising");
}

map<int, float> TSysfsGpioNew::PublishInterval(){
    map<int, float> TempMap;
    if (Type == "watt_meter"){
        Power = 1000 * Multiplier * 3600.0 / interval;
        Total = (float) counts / Multiplier;
        TempMap[0] = Total;
        TempMap[1] = Power;
    }
    return TempMap;
}

TSysfsGpioNew::~TSysfsGpioNew(){
}
