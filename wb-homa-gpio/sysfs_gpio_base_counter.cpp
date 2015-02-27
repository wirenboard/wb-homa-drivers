#include"sysfs_gpio.h"
#include<iostream>
#include<memory>

using namespace std;

TSysfsGpioBaseCounter::TSysfsGpioBaseCounter(int gpio, bool inverted, string interrupt_edge, string type, int multiplier)
    : TSysfsGpio( gpio, inverted, interrupt_edge)
    , Type(type)
    , Multiplier(multiplier)
    , Total(0)
{   
    if (Type == WATT_METER) {
        if (InterruptEdge == "") SetInterruptEdge("rising");// if user forgot to set interrupt edge, set default rising
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
    int compare_value = (GetInverted()) ? 1: 0;
    if ((GetInterruptEdge() == "rising") && ( value == compare_value))
        return output_vector;
    compare_value = !compare_value;
    if ((GetInterruptEdge() == "falling") && (value == compare_value))
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
