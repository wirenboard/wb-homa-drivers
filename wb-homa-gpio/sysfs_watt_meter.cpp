#include"sysfs_gpio.h"
#include<iostream>
#include<memory>

using namespace std;

TSysfsWattMeter::TSysfsWattMeter(int gpio, bool inverted, string type, int multiplier)
    : TSysfsGpio( gpio, inverted)
    , Type(type)
    , Multiplier(multiplier)
    , Total(0)
{   
    SetInterruptEdge("rising");
}

TSysfsWattMeter::TSysfsWattMeter( TSysfsWattMeter&& tmp)
    : TSysfsGpio(std::move(tmp))
    , Type(tmp.Type)
    , Multiplier(tmp.Multiplier)
    , Total(tmp.Total)
{ 
}

vector<TPublishPair> TSysfsWattMeter::MetaType(){
    vector<TPublishPair> output_vector;
    output_vector.push_back(make_pair("/_current","power"));
    output_vector.push_back(make_pair("/_total", "power_consumption"));
    return output_vector;
}
vector<TPublishPair> TSysfsWattMeter::GpioPublish(){
    vector<TPublishPair> output_vector;
    int value = !!GetCachedValue();
    if ((GetInterruptEdge() == "rising") && ( value == 0))
        return output_vector;
    if ((GetInterruptEdge() == "falling") && (value == 1))
        return output_vector;
    GetInterval();
    Power = 1000 * Multiplier * 3600.0 / Interval;
    Total = (float) Counts / Multiplier;
    output_vector.push_back(make_pair("/_total", to_string(Total)));
    output_vector.push_back(make_pair("/_current", to_string(Power)));
    return output_vector;
}

TSysfsWattMeter::~TSysfsWattMeter(){
}
