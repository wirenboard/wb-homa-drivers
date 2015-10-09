#include "sysfs_gpio.h"
#include <iostream>
#include <memory>
#include <string.h>

using namespace std;

TSysfsGpioBaseCounter::TSysfsGpioBaseCounter(int gpio, bool inverted, string interrupt_edge, string type, int multiplier, int decimal_points_total, int decimal_points_current)
    : TSysfsGpio( gpio, inverted, interrupt_edge)
    , Type(type)
    , Multiplier(multiplier)
    , Total(0)
    , DecimalPointsTotal(decimal_points_total)
    , DecimalPointsCurrent(decimal_points_current) 
    , PrintedNULL(false)
{
    if (Type == WATT_METER) {
        Topic2 = "_current";
        Value_Topic2 = "power";
        Topic1 = "_total";
        Value_Topic1 = "power_consumption";
        ConvertingMultiplier = 1000;// convert  kW to W
        DecimalPointsCurrent = (DecimalPointsCurrent == -1) ? 2: DecimalPointsCurrent;
        DecimalPointsTotal = (DecimalPointsTotal == -1) ? 3: DecimalPointsTotal;
    } else if (Type == WATER_METER) {
        Topic2 = "_current";
        Value_Topic2 = "water_flow";
        Topic1 = "_total";
        Value_Topic1 = "water_consumption";
        ConvertingMultiplier = 1.0;
        DecimalPointsCurrent = (DecimalPointsCurrent == -1) ? 3: DecimalPointsCurrent;
        DecimalPointsTotal = (DecimalPointsTotal == -1) ? 2: DecimalPointsTotal;
    } else {
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

void TSysfsGpioBaseCounter::SetInitialValues(float total) 
{
   InitialTotal = total; 
   Total = total;
}

TPublishPair TSysfsGpioBaseCounter::CheckTimeInterval()
{
    if (Counts != 0) {
        std::chrono::steady_clock::time_point time_now=std::chrono::steady_clock::now();
        long long unsigned int measured_interval = std::chrono::duration_cast<std::chrono::microseconds> (time_now - Previous_Interrupt_Time).count();
        if (measured_interval > NULL_TIME_INTERVAL * Interval) {
            if (PrintedNULL) {
                return make_pair(string(""),string(""));
                } else {
                    Power = 0;
                    PrintedNULL = true;
                    return make_pair(Topic2, SetDecimalPoints(Power, DecimalPointsCurrent));
                }
        }
        if (measured_interval > CURRENT_TIME_INTERVAL * Interval) {
            PrintedNULL = false;
            Power = 3600.0 * 1000000 * ConvertingMultiplier/ (measured_interval * Multiplier);// convert microseconds to seconds, hours to seconds
            return make_pair(Topic2, SetDecimalPoints(Power, DecimalPointsCurrent));
        }
    }
        return make_pair(string(""),string(""));
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
{ vector<TPublishPair> output_vector; if (FirstTime) {
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
        Power = -1;
    else
        Power = 3600.0 * 1000000 * ConvertingMultiplier/ (Interval * Multiplier);// convert microseconds to seconds, hours to seconds
    Total = (float) Counts / Multiplier + InitialTotal;
    output_vector.push_back(make_pair(Topic1, SetDecimalPoints(Total, DecimalPointsTotal)));
    output_vector.push_back(make_pair(Topic2, SetDecimalPoints(Power, DecimalPointsCurrent)));
    return output_vector;
}

string TSysfsGpioBaseCounter::SetDecimalPoints(float value, int set_decimal_points)
{
    string output;
    char buff[10];
    sprintf(buff, "%.5f", value);
    int decimal_places = -1;
    bool stop = false;
    for(int i = 0; (i < strlen(buff)) && (!stop); i++) {
        if (buff[i] == '.' ) 
            decimal_places = 0;
        if (decimal_places > -1) {
            if (decimal_places > set_decimal_points) {
                buff[i] = '\0';
                stop = true;
            }
            decimal_places++;
        }
    }
    output = buff;
    return output;
}

TSysfsGpioBaseCounter::~TSysfsGpioBaseCounter()
{
}
