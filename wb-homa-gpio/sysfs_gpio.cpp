#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include "sysfs_gpio.h"
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>
#include<string.h>
#include<errno.h>

using namespace std;

TSysfsGpio::TSysfsGpio(int gpio, bool inverted)
    : Gpio(gpio)
    , Inverted(inverted)
    , Exported(false)
    , InterruptionSupport(false)
    , CachedValue(-1)
    , FileDes(-1)
    , g_mutex()
    , in(false)
    , Debouncing(true)
    , interval(0)
    , counts(0)
{   
    ev_d.events= EPOLLET;

    //~ if (Export() == 0) {
        //~ Exported = true;
    //~ } else {
        //~ Exported = false;
    //~ }

}

/*TSysfsGpio::TSysfsGpio(const TSysfsGpio& other) 
    : Gpio(other.Gpio)
    , Inverted(other.Inverted)
    , Exported(other.Exported)
    , InterruptionSupport(other.InterruptionSupport)
    , CachedValue(other.CachedValue)
    , FileDes(-1)
    , g_mutex()
    , in(other.in)
    , interval(other.interval)
    , counts(other.counts)
{
    this->ev_d.events= EPOLLET;
    this->ev_d.data.fd=other.ev_d.data.fd;
}*/

TSysfsGpio::TSysfsGpio( TSysfsGpio&& tmp)
    : Gpio(tmp.Gpio)
    , Inverted(tmp.Inverted)
    , Exported(tmp.Exported)
    , InterruptionSupport(tmp.InterruptionSupport)
    , CachedValue(tmp.CachedValue)
    , FileDes(tmp.FileDes)
    , g_mutex()
    , in(tmp.in)
    , Debouncing(true)
    , interval(tmp.interval)
    , counts(tmp.counts)
{ 
    cout << "MOVING NOW GPIO " << Gpio << endl;
    ev_d.events = EPOLLET;
    ev_d.data.fd = tmp.ev_d.data.fd;
    tmp.FileDes = -1;
    tmp.ev_d.data.fd= -1;
}


int TSysfsGpio::Export()
{
    string export_str = "/sys/class/gpio/export";
    string path_to_value= "/sys/class/gpio/gpio" + to_string(Gpio) + "/value";
    ofstream exportgpio(export_str.c_str());
    if (!exportgpio.is_open()){
        cerr << " OPERATION FAILED: Unable to export GPIO"<< Gpio <<" ."<< endl;
        return -1;
    }

    exportgpio << Gpio ; //write GPIO number to export
    exportgpio << "\n";
    exportgpio.close(); //close export file
    // open file descriptro of value file and keep it in FileDes and ev_data.fd
    int _fd=open(path_to_value.c_str(), O_RDWR | O_NONBLOCK );
    if (_fd <= 0 ) {
        cout << strerror(errno) << endl;
        cout << "cannot open value for GPIO" << Gpio <<endl;
    }
    FileDes=_fd;
    ev_d.data.fd=_fd;
    cout << "exported " << Gpio << " filedes is " << FileDes << endl;
    
    Exported = true;
    
    return 0;
}

int TSysfsGpio::Unexport()
{
    string unexport_str = "/sys/class/gpio/unexport";
    ofstream unexportgpio(unexport_str.c_str()); //Open unexport file
    if (!unexportgpio.is_open()){
        cerr << " OPERATION FAILED: Unable to unexport GPIO"<< Gpio <<" ."<< endl;
        return -1;
    }

    unexportgpio << Gpio ; //write GPIO number to unexport
    unexportgpio.close(); //close unexport file
    cout << "unexported " << Gpio << endl;
    if (FileDes >= 0 ) {
        close (FileDes) ;
    }
    return 0;
}

int TSysfsGpio::SetDirection(bool input)
{
    cerr << "DEBUG:: gpio=" << Gpio << " SetDirection() input= " << input << endl;

    string setdir_str ="/sys/class/gpio/gpio";
    setdir_str += to_string(Gpio) + "/direction";

    ofstream setdirgpio(setdir_str.c_str()); // open direction file for gpio
    if (setdirgpio < 0){
        cout << " OPERATION FAILED: Unable to set direction of GPIO"<< Gpio <<" ."<< endl;
        setdirgpio.close();
        return -1;
    }

    //write direction to direction file
    if (input) {
        setdirgpio << "in";
        in=true;
    } else {
        setdirgpio << "out";
        in=false;
    }

    setdirgpio.close(); // close direction file
    return 0;
}

int TSysfsGpio::SetValue(int value)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    cerr << "DEBUG:: gpio=" << Gpio << " SetValue()  value= " << value << "pid is " << getpid() << "filedis is " << FileDes << endl;
    char buf= '0';
    string setval_str = "/sys/class/gpio/gpio";
    setval_str +=  to_string(Gpio) + "/value";
    //ofstream setvalgpio(setval_str.c_str()); 
        
    int prep_value = PrepareValue(value);
    if (prep_value == 1 ) buf='1';
    //setvalgpio << prep_value ;
    //setvalgpio.close();
    if (lseek(FileDes, 0, SEEK_SET) == -1 ){
        cout << "lseek returned -1" << endl;
    }
    if (write(FileDes, &buf, sizeof(char)) <= 0 ){//write value to value file, FileDes has been initialized in Export
        cout << strerror(errno);
        cout << " OPERATION SetValue FAILED: Unable to set the value of GPIO"<< Gpio << " ."<< "filedis is " << FileDes <<  endl;
        //setvalgpio.close();
        return -1;
    }


    CachedValue = prep_value;
    return 0;
}

int TSysfsGpio::GetValue(){
    lock_guard<mutex> lock(g_mutex);
    char buf='0';
    //string val;
    int ret;
    string Getval_str = "/sys/class/gpio/gpio";
    Getval_str += to_string(Gpio) + "/value";
    //ifstream Getvalgpio(Getval_str.c_str());
    if (lseek(FileDes, 0, SEEK_SET) == -1 ) {
        cout << "lseek returned -1 " << endl;
    }
    if (read(FileDes, &buf, sizeof(char)) <= 0){ //read gpio value
        cout << " OPERATION GetValue FAILED: Unable to Get value of GPIO"<< Gpio <<" filedes is " << FileDes << "pid is " << getpid() << "."<< endl;
        perror("error is ");
        //Getvalgpio.close();
        return -1;
    }
    //Getvalgpio >> val ;  

    if (buf != '0') {
        ret = PrepareValue(1);
    } else {
        ret = PrepareValue(0);
    }
    CachedValue = ret;
    //Getvalgpio.close();
    return ret;
}
int TSysfsGpio::InterruptionUp() {
    string path="/sys/class/gpio/gpio";
    string path_to_edge = path + to_string(Gpio) + "/edge";
    string path_to_value=path + to_string(Gpio) + "/value";
    if ( access( path_to_edge.c_str(), F_OK) == -1 ) return -1;
    if (in) {// if direction is input we will write to edge file
        ofstream setInterruption(path_to_edge.c_str());
        if (!setInterruption.is_open()){
            cout << " OPERATION FAILED: Unable to set the Interruption of GPIO"<< Gpio <<" ."<< endl;
            setInterruption.close();
            return -1;
        }
        setInterruption << GetFront();
        setInterruption.close();
        InterruptionSupport = true;
    }else {
        InterruptionSupport= false;
    }
    
    return 0;
}

 bool TSysfsGpio::GetInterruptionSupport() {
    return InterruptionSupport;
}

int TSysfsGpio::GetFileDes() {
    return FileDes;
}

struct epoll_event& TSysfsGpio::GetEpollStruct() {
    return ev_d;
}

bool TSysfsGpio::IsDebouncing(){
    if (counts == 0) return false; 
    std::chrono::steady_clock::time_point time_now=std::chrono::steady_clock::now();
    long long inter = std::chrono::duration_cast<std::chrono::microseconds> (time_now - previous).count();
    if ( inter > 1000) 
        Debouncing = false;
    else
        Debouncing = true;
    return Debouncing;
}

void TSysfsGpio::GetInterval(){
    if (counts != 0) {
        std::chrono::steady_clock::time_point time_now=std::chrono::steady_clock::now();
        interval = std::chrono::duration_cast<std::chrono::microseconds> (time_now - previous).count();
        counts++;
        previous=time_now;
    }else {
        counts=1;
        previous = std::chrono::steady_clock::now();
    } 
    PublishInterval();
    cout << "DEBUG: GPIO:" << Gpio << "interval= " << interval << "counts= " << counts << endl;
}

map<int, float>  TSysfsGpio::PublishInterval(){
    map<int,float> Map; 
    return Map; 
}
string TSysfsGpio::GetFront(){
    if ( front == "") 
        return "both";
    else 
        return front;
}
void TSysfsGpio::SetFront(string s){
    front = s;
}
TSysfsGpio::~TSysfsGpio(){
    if ( FileDes >= 0 ) {
        close(FileDes);
    }


    //~ if (IsExported()) {
        //~ Unexport();
    //~ }
}
