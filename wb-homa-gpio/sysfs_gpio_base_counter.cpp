#include "sysfs_gpio.h"
#include <iostream>
#include <memory>

using namespace std;

TSysfsGpioBaseCounter::TSysfsGpioBaseCounter(int gpio, bool inverted, string interrupt_edge, string type, int multiplier)
    : TSysfsGpio( gpio, inverted, interrupt_edge)
    , Type(type)
    , Multiplier(multiplier)
    , Total(0)
{   
    bool succes = false; 
    if (Type == WATT_METER) {
        Topic2 = "/_current";
        Value_Topic2 = "power";
        Topic1 = "/_total";
        Value_Topic1 = "power_consumption";
        ConvertingMultiplier = 1000;// convert  kW to W
        succes = true;
    }
    if (Type == WATER_METER) {
        Topic2 = "/_current";
        Value_Topic2 = "water_flow";
        Topic1 = "/_total";
        Value_Topic1 = "water_consumption";
        ConvertingMultiplier = 1.0;
        succes = true;
    }
    if (!succes) {
        cerr << "Uknown gpio type\n";
        exit(EXIT_FAILURE);
    }
}

TSysfsGpioBaseCounter::TSysfsGpioBaseCounter( TSysfsGpioBaseCounter&& tmp)
    : TSysfsGpio(std::move(tmp))
    , Type(tmp.Type)
    , Multiplier(tmp.Multiplier)
    , Total(tmp.Total)
{ 
}

int TSysfsGpioBaseCounter::InterruptUp()
{
    int sum = 0;
    int testing_number = 10;
    if (GetInterruptEdge() == "both") {
        for (int i = 0; i < testing_number ; i++)
            sum += GetValue();
        if ( sum < testing_number) 
            SetInterruptEdge("rising");  
        else
            SetInterruptEdge("falling");
    }
    return TSysfsGpio::InterruptUp();
}

vector<TPublishPair> TSysfsGpioBaseCounter::MetaType()
{
    vector<TPublishPair> output_vector;
    output_vector.push_back(make_pair(Topic1, Value_Topic1));
    output_vector.push_back(make_pair(Topic2, Value_Topic2));
    return output_vector;
}
vector<TPublishPair> TSysfsGpioBaseCounter::GpioPublish()
{
    vector<TPublishPair> output_vector;
    if (FirstTime) {
        FirstTime = false;
        return output_vector;
    }
    int cached_value = GetCachedValue();
    int value = GetValue();
    if (value != cached_value) {// check what front we got here
        if ((GetInterruptEdge() == "rising") && (value == 0))
            return output_vector;
        if ((GetInterruptEdge() == "falling") && (value == 1))
            return output_vector;
    }
    if (!GetInterval()) {// ignore repeated interrupts of one impulse 
        return output_vector;
    }
    // in other cases we have correct interrupt and handle it
    if (Interval == 0) 
        Power =-1;
    else 
        Power = 3600.0 * 1000000 * ConvertingMultiplier/ (Interval * Multiplier);// convert microseconds to seconds, hours to seconds
    Total = (float) Counts / Multiplier;
    output_vector.push_back(make_pair(Topic1, to_string(Total)));
    output_vector.push_back(make_pair(Topic2, to_string(Power)));
    return output_vector;
}

TSysfsGpioBaseCounter::~TSysfsGpioBaseCounter()
{
}
