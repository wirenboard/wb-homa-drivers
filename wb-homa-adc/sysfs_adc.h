#pragma once
#include <string>
#include <exception>
#include <memory>
#include<vector>

using namespace std;

struct TMUXChannel{
    std::string Id;
    float Multiplier = 1.0;
};
struct TChannel{
    int AveragingWindow = 10;
    int PollInterval;
    int ChannelNumber = 1;
    int MinSwitchIntervalMs = 0;
    string Type = "";
    vector<int> Gpios;
    vector<TMUXChannel> Mux;
};
 

class TSysfsAdcChannel;

class TAdcException: public std::exception {
public:
    TAdcException(std::string _message): message(_message) {}
    const char* what () const throw ()
    {
        return ("Adc error: " + message).c_str();
    }
private:
    std::string message;
};

class TSysfsAdc
{
public:
    TSysfsAdc(const std::string& sysfs_dir = "/sys", bool debug = false, const TChannel& channel_config = TChannel ());
    TSysfsAdcChannel GetChannel(int i);
    int ReadValue();
protected:
    virtual int GetValue(int index) = 0;
    int AveragingWindow;
    bool Debug;
    bool Initialized;
    std::string SysfsDir;
    ifstream AdcValStream;
    friend class TSysfsAdcChannel;
    TChannel ChannelConfig;
};

class TSysfsAdcMux : public TSysfsAdc{
    public : 
        TSysfsAdcMux(const std::string& sysfs_dir = "/sys/", bool debug = false, const TChannel& channel_config = TChannel ());
    private:
        int GetValue(int index);
        void InitMux();
        void InitGPIO(int gpio);
        void SetGPIOValue(int gpio, int value);
        std::string GPIOPath(int gpio, const std::string& suffix) const;
        void MaybeWaitBeforeSwitching();
        void SetMuxABC(int n);
        int MinSwitchIntervalMs;
        int CurrentMuxInput;
        struct timespec PrevSwitchTS;
        int GpioMuxA;
        int GpioMuxB;
        int GpioMuxC;
};

class TSysfsAdcPhys: public TSysfsAdc{
    public :
        TSysfsAdcPhys(const std::string& sysfs_dir = "/sys/", bool debug = false, const TChannel& channel_config = TChannel ());
    private : 
    int GetValue(int index);
};
 
struct TSysfsAdcChannelPrivate {
    ~TSysfsAdcChannelPrivate() { if (Buffer) delete[] Buffer; }
    std::shared_ptr<TSysfsAdc> Owner;
    int Index;
    std::string Name;
    int* Buffer = 0;
    double Sum = 0;
    bool Ready = false;
    int Pos = 0;
    float Multiplier = 1.0;
};


class TSysfsAdcChannel
{
public:
    int GetValue();
    const std::string& GetName() const;
    inline float GetMultiplier () { return d->Multiplier; }
private:
    TSysfsAdcChannel(TSysfsAdc* owner, int index, const std::string& name, float multiplier);
    std::shared_ptr<TSysfsAdcChannelPrivate> d;
    friend class TSysfsAdc;
};
