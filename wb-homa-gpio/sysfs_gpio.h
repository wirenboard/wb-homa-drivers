#pragma once
#include <string>
#include<sys/epoll.h>

using namespace std;

class TSysfsGpio
{
public:
    explicit TSysfsGpio(int gpio, bool inverted = false);
    ~TSysfsGpio();
    int Export();
    int Unexport();
    int SetDirection(bool input = true); // Set GPIO Direction
    inline int SetInput() { return SetDirection(true); };
    inline int SetOutput() { return SetDirection(false); };

    int SetValue(int val);
    // returns GPIO value (0 or 1), or negative number in case of error
    int GetValue();
    int InterruptUp();
    bool getInterruptSupport() ;
    int getFileDes();
    struct epoll_event* getEpollStruct() ;
        // returns GPIO value (0 or 1) or default in case of error
    inline int GetValueOrDefault(int def = 0)
        { int val =  GetValue(); return (val < 0) ? def : val; }

    inline int GetGpio() { return Gpio; };
    inline bool IsExported() { return Exported; };
    inline void SetInverted (bool inverted = true) {Inverted = inverted;};

    //returns GPIO values (0 or 1) or negative in case of error
    inline int GetCachedValue() { return CachedValue; };
    inline int GetCachedValueOrDefault(int def = 0) {  return (CachedValue < 0) ? def : CachedValue; }


private:
    // invert value if needed
    inline int PrepareValue(int value) { return value ^ Inverted;};
    private:
    int Gpio;
    bool Inverted;
    bool Exported;
    bool InterruptSupport;
    struct epoll_event ev_d;

    int CachedValue;

};
