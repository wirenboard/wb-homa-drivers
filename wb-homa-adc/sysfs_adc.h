#pragma once
#include <string>
#include <exception>
#include <memory>
#include <vector>
#include "imx233.h"
#define OHM_METER "ohm_meter"
#define DELAY 1
#define ADC_OLD_SCALE 0.451660156 // default scale for file "in_voltageNUMBER_scale"
#define VALUE_MAXIMUM 4095
using namespace std;

struct TMUXChannel // config for mux channel
{
    std::string Id;
    float Multiplier = 1.0;
    std::string Type = "";
    int Current = 40;// current in uA
    int Resistance1 = 1000;// resistance in Ohm
    int Resistance2 = 1000;// resistance in Ohm
    int MuxChannelNumber = 0;// ADC channel number
    int ReadingsNumber = 10; // number of reading value during one selection
    int DecimalPlaces = 3;
};
struct TChannel
{
    int AveragingWindow = 10;
    int PollInterval;
    int ChannelNumber = 1;//Number of TMUXChannels in vector Mux
    int MinSwitchIntervalMs = 0;
    string Type = "";
    vector<int> Gpios;
    vector<TMUXChannel> Mux;
};
 

class TSysfsAdcChannel;

class TAdcException: public std::exception 
{
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
    ~TSysfsAdc();
    std::shared_ptr<TSysfsAdcChannel> GetChannel(int i);
    int ReadValue();
    inline int GetNumberOfChannels() { return NumberOfChannels; };
    double ScaleFactor;// Factor that comes from calculating ratio of ADC_NEW_SCALE to  ADC_OLD_SCALE, ADC_NEW_SCALE is the maximum scale from file "in_voltageNUMBER_scale_available"  
protected:
    virtual int GetRawValue(int index) = 0;
    int AveragingWindow;
    bool Debug;
    bool Initialized;
    std::string SysfsDir;
    ifstream AdcValStream;
    friend class TSysfsAdcChannel;
    TChannel ChannelConfig;
    int NumberOfChannels;
};

class TSysfsAdcMux : public TSysfsAdc // class, that handles mux channels
{
    public : 
        TSysfsAdcMux(const std::string& sysfs_dir = "/sys/", bool debug = false, const TChannel& channel_config = TChannel ());
    private:
        int GetRawValue(int index);
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

class TSysfsAdcPhys: public TSysfsAdc
{
    public :
        TSysfsAdcPhys(const std::string& sysfs_dir = "/sys/", bool debug = false, const TChannel& channel_config = TChannel ());
    private : 
    int GetRawValue(int index);
};
 
struct TSysfsAdcChannelPrivate 
{
    ~TSysfsAdcChannelPrivate() { if (Buffer) delete[] Buffer; }
    std::shared_ptr<TSysfsAdc> Owner;
    int Index;
    std::string Name;
    int* Buffer = 0;
    double Sum = 0;
    bool Ready = false;
    int Pos = 0;
    int ReadingsNumber;
    int ChannelAveragingWindow;

}; 
class TSysfsAdcChannel 
{ 
    public: 
        int GetAverageValue();
        virtual float GetValue(); 
        const std::string& GetName() const;
        virtual std::string GetType();
        TSysfsAdcChannel(TSysfsAdc* owner, int index, const std::string& name, int readings_number, int decimal_places);
        TSysfsAdcChannel(TSysfsAdc* owner, int index, const std::string& name, int readings_number,int decimal_places, float multiplier);
        int DecimalPlaces;
    protected:
        std::shared_ptr<TSysfsAdcChannelPrivate> d;
        friend class TSysfsAdc;
    private:
        float Multiplier;
};

class TSysfsAdcChannelRes : public TSysfsAdcChannel// class, that measures resistance
{
    public : 
         TSysfsAdcChannelRes(TSysfsAdc* owner, int index, const std::string& name, int readings_number, int decimal_places, int current, int resistance1, int resistance2);
         float GetValue();
         std::string GetType();
         void SetUpCurrentSource();
         void SwitchOffCurrentSource();
    private:
        int Current;
        int Resistance1;
        int Resistance2;
        std::string Type;
        unsigned int Ctrl2_val;
};
