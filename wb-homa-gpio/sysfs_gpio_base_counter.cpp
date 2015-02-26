#include"sysfs_gpio.h"
#include<iostream>
#include<memory>

using namespace std;

TSysfsGpioBaseCounter::TSysfsGpioBaseCounter(int gpio, bool inverted, string type, int multiplier)
    : TSysfsGpio( gpio, inverted)
    , Type(type)
    , Multiplier(multiplier)
    , Total(0)
{   
    if (Type == WATT_METER) {
        SetInterruptEdge("rising");
        Topic2 = "/_current";
        Value_Topic2 = "power";
        Topic1 = "/_total";
        Value_Topic1 = "power_consumption";
    }
}

TSysfsGpioBaseCounter::TSysfsGpioBaseCounter( TSysfsGpioBaseCounter&& tmp)
    : TSysfsGpio(std::move(tmp))
    , Type(tmp.Type)
    , Multiplier(tmp.Multiplier)
    , Total(tmp.Total)
{ 
}

vector<TPublishPair> TSysfsGpioBaseCounter::MetaType(){
    vector<TPublishPair> output_vector;
    output_vector.push_back(make_pair(Topic1, Value_Topic1));
    output_vector.push_back(make_pair(Topic2, Value_Topic2));
    return output_vector;
}
vector<TPublishPair> TSysfsGpioBaseCounter::GpioPublish(){
    vector<TPublishPair> output_vector;
    int value = !!GetCachedValue();
    if ((GetInterruptEdge() == "rising") && ( value == 0))
        return output_vector;
    if ((GetInterruptEdge() == "falling") && (value == 1))
        return output_vector;
    GetInterval();
    Power = 1000 * Multiplier * 3600.0 / Interval;
    Total = (float) Counts / Multiplier;
    output_vector.push_back(make_pair(Topic1, to_string(Total)));
    output_vector.push_back(make_pair(Topic2, to_string(Power)));
    return output_vector;
}

TSysfsGpioBaseCounter::~TSysfsGpioBaseCounter(){
}
