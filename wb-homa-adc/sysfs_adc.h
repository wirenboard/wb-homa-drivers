#pragma once
#include <string>
#include <exception>

class TSysfsADCChannel;

class TSysfsADCException: public std::exception {
public:
    TSysfsADCException(std::string _message): message(_message) {}
    const char* what () const throw ()
    {
        return ("ADC error: " + message).c_str();
    }
private:
    std::string message;
};

class TSysfsADC
{
public:
    TSysfsADC(const std::string& sysfs_dir = "/sys");
    TSysfsADCChannel GetChannel(const std::string& channel_name);
private:
    int GetValue(int index);
    void InitMux();
    void InitGPIO(int gpio);
    void SetGPIOValue(int gpio, int value);
    std::string GPIOPath(int gpio, const std::string& suffix) const;
    void SetMuxABC(int n);
    bool Initialized;
    std::string SysfsDir;
    friend class TSysfsADCChannel;
};

class TSysfsADCChannel
{
public:
    int GetValue() const { return Owner->GetValue(Index); }
    const std::string& GetName() const { return Name; }
private:
    TSysfsADCChannel(TSysfsADC* owner, int index, const std::string& name)
        : Owner(owner), Index(index), Name(name) {}
    TSysfsADC* Owner;
    int Index;
    std::string Name;
    friend class TSysfsADC;
};
