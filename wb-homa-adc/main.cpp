#include <cstdlib>
#include <iostream>
#include "sysfs_adc.h"

std::string GetSysfsPrefix()
{
    const char* prefix = getenv("WB_SYSFS_PREFIX");
    return prefix ? prefix : "/sys";
}

int main(int argc, char **argv)
{
    TSysfsADC adc(GetSysfsPrefix());
    auto channel = adc.GetChannel(argc < 2 ? "r1" : argv[1]);
    std::cout << "value: " << channel.GetValue() << std::endl;
    return 0;
}
