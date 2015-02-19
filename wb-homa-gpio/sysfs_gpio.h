#pragma once
#include <string>
#include<sys/epoll.h>
#include<mutex>
#include<chrono>
#include<map>
#include<memory>

using namespace std;

class TSysfsGpio
{
public:
    explicit TSysfsGpio(int gpio, bool inverted = false);
    TSysfsGpio(const TSysfsGpio& other) = delete;
    TSysfsGpio(TSysfsGpio&& tmp);
    ~TSysfsGpio();
    int Export();
    int Unexport();
    int SetDirection(bool input = true); // Set GPIO Direction
    inline int SetInput() { return SetDirection(true); };
    inline int SetOutput() { return SetDirection(false); };

    int SetValue(int val);
    // returns GPIO value (0 or 1), or negative number in case of error
    int GetValue();
    
    int InterruptionUp();// trying to write to edge file and checking is it input gpio
    
    //returns true if gpio support interruption
    bool GetInterruptionSupport() ;
    //returns gpio value file description
    int GetFileDes();
    struct epoll_event& GetEpollStruct() ;
        // returns GPIO value (0 or 1) or default in case of error
    inline int GetValueOrDefault(int def = 0)
        { int val =  GetValue(); return (val < 0) ? def : val; }

    inline int GetGpio() { return Gpio; };
    inline bool IsExported() { return Exported; };
    inline void SetInverted (bool inverted = true) {Inverted = inverted;};

    //returns GPIO values (0 or 1) or negative in case of error
    inline int GetCachedValue() { return CachedValue; };
    inline int GetCachedValueOrDefault(int def = 0) {  return (CachedValue < 0) ? def : CachedValue; }
        
    virtual map <int, float> PublishInterval(); //getting what nessesary for  publishing to the mqtt broker according to type
    virtual void GetInterval();// measure time interval between interruptions
    string GetFront(); // returning front of the impulse, that we're trying to catch
    void SetFront(string s); // set front of the impulse
    bool IsDebouncing(); // if interval between two interruptions is less than 1 milisecond it will return true;

private:
    // invert value if needed
    inline int PrepareValue(int value) { return value ^ Inverted;};
    private:
    int Gpio;
    bool Inverted;
    bool Exported;
    bool InterruptionSupport;
    struct epoll_event ev_d;

    int CachedValue;
    int FileDes;
    mutable mutex g_mutex;
    bool in;//direction true=in false=out
    std::chrono::steady_clock::time_point previous;
    bool Debouncing;
protected: 
    long long unsigned int interval;
    long unsigned int counts;
    string front;

};

class TSysfsGpioNew : public TSysfsGpio {
    public:
        explicit TSysfsGpioNew(int gpio, bool inverted, string type, int multiplier);
        TSysfsGpioNew(const TSysfsGpioNew& other) = delete;
        TSysfsGpioNew(TSysfsGpioNew&& tmp) ;
        ~TSysfsGpioNew();
        map<int, float> PublishInterval();
        void PublishWattMeterInterval();
        void InitWattMeter();
        inline float GetTotal( ){ return Total; }; // return total enegry
        inline float GetPower() { return Power; }; // return current power
    
    private:
        string Type;
        int Multiplier;
        float Total;
        float Power;

};
