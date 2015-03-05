#pragma once
#include <string>
#include <exception>
#include <memory>
#include<vector>

using namespace std;

struct TMUXChannel{
    std::string Id;
    float Multiplier;
};


class TSysfsADCChannel;

class TADCException: public std::exception {
public:
    TADCException(std::string _message): message(_message) {}
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
    TSysfsADC(const std::string& sysfs_dir = "/sys", int averaging_window = 10,
              int min_switch_interval_ms = 0, bool debug = false, vector<int> gpios = vector<int> (), vector<TMUXChannel> mux =vector<TMUXChannel> ());
    TSysfsADCChannel GetChannel(int i);
private:
    int GetValue(int index);
    void InitMux();
    void InitGPIO(int gpio);
    void SetGPIOValue(int gpio, int value);
    std::string GPIOPath(int gpio, const std::string& suffix) const;
    void MaybeWaitBeforeSwitching();
    void SetMuxABC(int n);
    int AveragingWindow;
    int MinSwitchIntervalMs;
    bool Debug;
    bool Initialized;
    std::string SysfsDir;
    int CurrentMuxInput;
    struct timespec PrevSwitchTS;
    int GpioMuxA;
    int GpioMuxB;
    int GpioMuxC;
    ifstream AdcValStream;
    friend class TSysfsADCChannel;
    vector<TMUXChannel> Mux;
};

struct TSysfsADCChannelPrivate {
    ~TSysfsADCChannelPrivate() { if (Buffer) delete[] Buffer; }
    TSysfsADC* Owner;
    int Index;
    std::string Name;
    int* Buffer = 0;
    double Sum = 0;
    bool Ready = false;
    int Pos = 0;
    float Multiplier = 1.0;
};


class TSysfsADCChannel
{
public:
    int GetValue();
    const std::string& GetName() const;
    inline float GetMultiplier () { return d->Multiplier; }
private:
    TSysfsADCChannel(TSysfsADC* owner, int index, const std::string& name, float multiplier);
    std::shared_ptr<TSysfsADCChannelPrivate> d;
    friend class TSysfsADC;
};
