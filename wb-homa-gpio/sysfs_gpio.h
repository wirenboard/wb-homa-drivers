#pragma once
#include <string>
#include <sys/epoll.h>
#include <mutex>
#include <chrono>
#include <vector>
#include <memory>
#include <utility>
#include <iostream>

#define WATT_METER "watt_meter"
#define WATER_METER "water_meter"
#define MICROSECONDS_DELAY 200000
#define CURRENT_TIME_INTERVAL 1
#define NULL_TIME_INTERVAL 100

using namespace std;

typedef pair<string, string> TPublishPair;
class TSysfsGpio
{
public:
    explicit TSysfsGpio(int gpio, bool inverted = false,string interrupt_edge = "");
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
    
    virtual int InterruptUp();// trying to write to edge file and checking is it input gpio
    
    //returns true if gpio support interruption
    bool GetInterruptSupport() ;
    //returns gpio value file description
    int GetFileDes();
    struct epoll_event& GetEpollStruct() ;
        // returns GPIO value (0 or 1) or default in case of error
    inline int GetValueOrDefault(int def = 0)
        { int val =  GetValue(); return (val < 0) ? def : val; }

    inline int GetGpio() { return Gpio; };
    inline bool IsExported() { return Exported; };
    inline void SetInverted (bool inverted = true) {Inverted = inverted;};
    inline bool GetInverted () { return Inverted; }

    //returns GPIO values (0 or 1) or negative in case of error
    inline int GetCachedValue() { return CachedValue; };
    inline void SetCachedValue(int cached) { CachedValue = cached; }; // sometimes we should save written cached value
    inline int GetCachedValueOrDefault(int def = 0) {  return (CachedValue < 0) ? def : CachedValue; }
        
    virtual vector<TPublishPair> MetaType(); // what publish to meta/type 
    virtual vector<TPublishPair> GpioPublish(); //getting what nessesary for  publishing to controls
    bool GetInterval();// measure time interval between interruptions, return false if interval between interrupts less then specified delay
    string GetInterruptEdge(); // returning front of the impulse, that we're trying to catch
    void SetInterruptEdge(string s); // set front of the impulse
    bool IsDebouncing(); // if interval between two interruptions is less than 1 milisecond it will return true;
    inline int GetCounts() { return Counts; }
    virtual void SetInitialValues(float total);
    virtual TPublishPair CheckTimeInterval(); // check if metter is alive
protected:
// invert value if needed
    inline int PrepareValue(int value) { return value ^ Inverted;};

private:
        private:
    int Gpio;
    bool Inverted;
    bool Exported;
    bool InterruptSupport;
    struct epoll_event Ev_d;

    int CachedValue;
    int FileDes;
    mutable mutex G_mutex;
    bool In;//direction true = in false = out
    bool Debouncing;
protected: 
    std::chrono::steady_clock::time_point Previous_Interrupt_Time;
    long long unsigned int Interval;
    long unsigned int Counts;
    string InterruptEdge;
    bool FirstTime;

};

class TSysfsGpioBaseCounter : public TSysfsGpio 
{
    public:
        explicit TSysfsGpioBaseCounter(int gpio, bool inverted, string interrupt_edge, string type, int multiplier, int decimal_points_total, int decimal_points_current);
        TSysfsGpioBaseCounter(const TSysfsGpioBaseCounter& other) = delete;
        TSysfsGpioBaseCounter(TSysfsGpioBaseCounter&& tmp) ;
        ~TSysfsGpioBaseCounter();
        vector<TPublishPair> MetaType();
        vector<TPublishPair> GpioPublish();   
        int InterruptUp();// if user didn't specify interrupt edge, method would figure it out
        void SetInitialValues(float total);// set total when starting
        TPublishPair CheckTimeInterval();
    
    private:
        string SetDecimalPlaces(float value, int decimal_points);
        string Type;
        int Multiplier;
        int ConvertingMultiplier;// multiplier that converts value to appropriate measuring unit according to meter type
        float Total;
        float Power;
        string Topic1;
        string Topic2;
        string Value_Topic1;
        string Value_Topic2;
        int DecimalPlacesTotal;
        int DecimalPlacesCurrent;
        float InitialTotal;
        bool PrintedNULL;

};
