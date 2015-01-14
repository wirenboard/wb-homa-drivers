#include <string>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <cmath>
#include <ctime>
#include <unistd.h>

#include "sysfs_adc.h"

namespace {
    struct ChannelName {
        int n;
        const char* name;
    };

    ChannelName channel_names[] = {
        {0, "adc0"},
        {1, "adc1"},
        {2, "adc2"},
        {3, "adc3"},
        {4, "adc4"},
        {5, "adc5"},
        {6, "adc6"},
        {7, "adc7"},
#if 0
        // later
        {0, "a1"},
        {0, "adc1"},
        {1, "a2"},
        {1, "adc2"},
        {2, "a3"},
        {2, "adc3"},
        {3, "a4"},
        {3, "adc4"},
        {4, "r1"},
        {5, "r4"},
        {6, "r2"},
        {7, "r3"},
#endif
        {-1, 0}
    };

    int GetChannelIndex(const std::string& name)
    {
        std::string locase_name = name;
        std::transform(locase_name.begin(), locase_name.end(), locase_name.begin(), ::tolower);
        for (ChannelName* name_item = channel_names; name_item->n >= 0; ++name_item) {
            if (locase_name == name_item->name)
                return name_item->n;
        }
        throw TADCException("invalid channel name " + name);
    }

    int GetGPIOFromEnv(const std::string& name)
    {
        char* s = getenv(name.c_str());
        if (!s)
            throw TADCException("Environment variable not set: " + name);
        try {
            return std::stoi(s);
        } catch (std::exception) {
            throw TADCException("Invalid value of environment variable '" + name + "': " + s);
        }
    }
};

TSysfsADC::TSysfsADC(const std::string& sysfs_dir, int averaging_window,
                     int min_switch_interval_ms, bool debug)
    : AveragingWindow(averaging_window),
      MinSwitchIntervalMs(min_switch_interval_ms),
      Debug(debug),
      Initialized(false),
      SysfsDir(sysfs_dir),
      CurrentMuxInput(-1),
      AdcValStream(SysfsDir + "/bus/iio/devices/iio:device0/in_voltage1_raw")
{
    GpioMuxA = GetGPIOFromEnv("WB_GPIO_MUX_A");
    GpioMuxB = GetGPIOFromEnv("WB_GPIO_MUX_B");
    GpioMuxC = GetGPIOFromEnv("WB_GPIO_MUX_C");
    if (AdcValStream < 0) {
        throw TADCException("error opening sysfs ADC file");
    }

}

TSysfsADCChannel TSysfsADC::GetChannel(const std::string& channel_name)
{
    // TBD: should pass chain_alias also (to be used instead of Name for the channel)
    return TSysfsADCChannel(this, GetChannelIndex(channel_name), channel_name);
}

int TSysfsADC::GetValue(int index)
{
    SetMuxABC(index);
    int val;
    AdcValStream.seekg(0);
    AdcValStream >> val;
    return val;
}

void TSysfsADC::InitMux()
{
    if (Initialized)
        return;
    InitGPIO(GpioMuxA);
    InitGPIO(GpioMuxB);
    InitGPIO(GpioMuxC);
    Initialized = true;
}

void TSysfsADC::InitGPIO(int gpio)
{
    std::string gpio_direction_path = GPIOPath(gpio, "/direction");
    std::ofstream setdirgpio(gpio_direction_path);
    if (!setdirgpio) {
        std::ofstream exportgpio(SysfsDir + "/class/gpio/export");
        if (!exportgpio)
            throw TADCException("unable to export GPIO " + std::to_string(gpio));
        exportgpio << gpio << std::endl;
        setdirgpio.clear();
        setdirgpio.open(gpio_direction_path);
        if (!setdirgpio)
            throw TADCException("unable to set GPIO direction");
    }
    setdirgpio << "out";
}

void TSysfsADC::SetGPIOValue(int gpio, int value)
{
    std::ofstream setvalgpio(GPIOPath(gpio, "/value"));
    if (!setvalgpio)
        throw TADCException("unable to set value of gpio " + std::to_string(gpio));
    setvalgpio << value << std::endl;
}

std::string TSysfsADC::GPIOPath(int gpio, const std::string& suffix) const
{
    return std::string(SysfsDir + "/class/gpio/gpio") + std::to_string(gpio) + suffix;
}

void TSysfsADC::MaybeWaitBeforeSwitching()
{
    if (MinSwitchIntervalMs <= 0)
        return;

    struct timespec tp;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tp) < 0)
        throw TADCException("unable to get timer value");

    if (CurrentMuxInput >= 0) { // no delays before the first switch
        double elapsed_ms = (tp.tv_sec - PrevSwitchTS.tv_sec) * 1000 +
            (tp.tv_nsec - PrevSwitchTS.tv_nsec) / 1000000;
        if (Debug)
            std::cerr << "elapsed: " << elapsed_ms << std::endl;
        if (elapsed_ms < MinSwitchIntervalMs) {
            if (Debug)
                std::cerr << "usleep: " << (MinSwitchIntervalMs - (int)elapsed_ms) * 1000 <<
                    std::endl;
            usleep((MinSwitchIntervalMs - (int)elapsed_ms) * 1000);
        }
    }
    PrevSwitchTS = tp;
}

void TSysfsADC::SetMuxABC(int n)
{
    InitMux();
    if (CurrentMuxInput == n)
        return;
    if (Debug)
        std::cerr << "SetMuxABC: " << n << std::endl;
    SetGPIOValue(GpioMuxA, n & 1);
    SetGPIOValue(GpioMuxB, n & 2);
    SetGPIOValue(GpioMuxC, n & 4);
    CurrentMuxInput = n;
    usleep(MinSwitchIntervalMs * 1000);
}

struct TSysfsADCChannelPrivate {
    ~TSysfsADCChannelPrivate() { if (Buffer) delete[] Buffer; }
    TSysfsADC* Owner;
    int Index;
    std::string Name;
    int* Buffer = 0;
    double Sum = 0;
    bool Ready = false;
    int Pos = 0;
};

TSysfsADCChannel::TSysfsADCChannel(TSysfsADC* owner, int index, const std::string& name)
    : d(new TSysfsADCChannelPrivate())
{
    d->Owner = owner;
    d->Index = index;
    d->Name = name;
    d->Buffer = new int[d->Owner->AveragingWindow](); // () initializes with zeros
}

int TSysfsADCChannel::GetValue()
{
    if (!d->Ready) {
        for (int i = 0; i < d->Owner->AveragingWindow; ++i) {
            int v = d->Owner->GetValue(d->Index);
            d->Buffer[i] = v;
            d->Sum += v;
        }
        d->Ready = true;
    } else {
        int v = d->Owner->GetValue(d->Index);
        d->Sum -= d->Buffer[d->Pos];
        d->Sum += v;
        d->Buffer[d->Pos++] = v;
        d->Pos %= d->Owner->AveragingWindow;
    }

    return round(d->Sum / d->Owner->AveragingWindow);
}

const std::string& TSysfsADCChannel::GetName() const
{
    return d->Name;
}
