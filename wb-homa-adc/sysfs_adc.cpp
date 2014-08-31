#include <string>
#include <algorithm>
#include <fstream>
#include <iostream>

#include "sysfs_adc.h"

namespace {
	const int WB_GPIO_MUX_A = 34;
	const int WB_GPIO_MUX_B = 33;
	const int WB_GPIO_MUX_C = 32;

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
            if (locase_name == name_item->name) {
                std::cout << "name: " << name << "; name_item->name: " << name_item->name << "; n: " << name_item->n << std::endl;
                return name_item->n;
            }
        }
        throw TSysfsADCException("invalid channel name " + name);
    }
};

TSysfsADC::TSysfsADC(const std::string& sysfs_dir)
    : Initialized(false), SysfsDir(sysfs_dir) {}

TSysfsADCChannel TSysfsADC::GetChannel(const std::string& channel_name)
{
    // TBD: should pass chain_alias also (to be used instead of Name for the channel)
    return TSysfsADCChannel(this, GetChannelIndex(channel_name), channel_name);
}

int TSysfsADC::GetValue(int index)
{
    SetMuxABC(index);
    std::ifstream getvaladc(SysfsDir + "/bus/iio/devices/iio:device0/in_voltage1_raw");

    if (getvaladc < 0)
        throw TSysfsADCException("unable to read ADC value");

    int val;
    getvaladc >> val;
    return val;
}

void TSysfsADC::InitMux()
{
    if (Initialized)
        return;
    InitGPIO(WB_GPIO_MUX_A);
    InitGPIO(WB_GPIO_MUX_B);
    InitGPIO(WB_GPIO_MUX_C);
    Initialized = true;
}

void TSysfsADC::InitGPIO(int gpio)
{
    std::string gpio_direction_path = GPIOPath(gpio, "/direction");
    std::ofstream setdirgpio(gpio_direction_path);
    if (!setdirgpio) {
        std::ofstream exportgpio(SysfsDir + "/class/gpio/export");
        if (!exportgpio)
            throw TSysfsADCException("unable to export GPIO " + std::to_string(gpio));
        exportgpio << gpio << std::endl;
        setdirgpio.clear();
        setdirgpio.open(gpio_direction_path);
        if (!setdirgpio)
            throw TSysfsADCException("unable to set GPIO direction");
    }
    setdirgpio << "out";
}

void TSysfsADC::SetGPIOValue(int gpio, int value)
{
    std::ofstream setvalgpio(GPIOPath(gpio, "/value"));
    if (!setvalgpio)
        throw TSysfsADCException("unable to set value of gpio " + std::to_string(gpio));
    setvalgpio << value << std::endl;
}

std::string TSysfsADC::GPIOPath(int gpio, const std::string& suffix) const
{
    return std::string(SysfsDir + "/class/gpio/gpio") + std::to_string(gpio) + suffix;
}

void TSysfsADC::SetMuxABC(int n)
{
    InitMux();
    std::cout << "n: " << n << std::endl;
    SetGPIOValue(WB_GPIO_MUX_A, n & 1);
    SetGPIOValue(WB_GPIO_MUX_B, n & 2);
    SetGPIOValue(WB_GPIO_MUX_C, n & 4);
}
