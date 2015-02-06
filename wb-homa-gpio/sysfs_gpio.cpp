#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include "sysfs_gpio.h"
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>

using namespace std;

TSysfsGpio::TSysfsGpio(int gpio, bool inverted)
    : Gpio(gpio)
    , Inverted(inverted)
    , Exported(false)
    , InterruptSupport(false)
    , CachedValue(-1)
{   
    ev_d.events= EPOLLET;

    //~ if (Export() == 0) {
        //~ Exported = true;
    //~ } else {
        //~ Exported = false;
    //~ }

}

int TSysfsGpio::Export()
{
    string export_str = "/sys/class/gpio/export";
    ofstream exportgpio(export_str.c_str());
    if (!exportgpio.is_open()){
        cerr << " OPERATION FAILED: Unable to export GPIO"<< Gpio <<" ."<< endl;
        return -1;
    }

    exportgpio << Gpio ; //write GPIO number to export
    exportgpio << "\n";
    exportgpio.close(); //close export file
    cout << "exported " << Gpio << endl;

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
    } else {
        setdirgpio << "out";
    }

    setdirgpio.close(); // close direction file
    return 0;
}

int TSysfsGpio::SetValue(int value)
{
    cerr << "DEBUG:: gpio=" << Gpio << " SetValue()  value= " << value << endl;
    string setval_str = "/sys/class/gpio/gpio";
    setval_str +=  to_string(Gpio) + "/value";
    ofstream setvalgpio(setval_str.c_str()); // open value file for gpio
    if (!setvalgpio.is_open()){
        cout << " OPERATION FAILED: Unable to set the value of GPIO"<< Gpio <<" ."<< endl;
        setvalgpio.close();
        return -1;
    }

    int prep_value = PrepareValue(value);
    setvalgpio << prep_value ;//write value to value file
    setvalgpio.close();// close value file

    CachedValue = prep_value;
    return 0;
}

int TSysfsGpio::GetValue(){

    string val;
    int ret;
    string getval_str = "/sys/class/gpio/gpio";
    getval_str += to_string(Gpio) + "/value";
    ifstream getvalgpio(getval_str.c_str());// open value file for gpio

    if (getvalgpio < 0){
        cout << " OPERATION FAILED: Unable to get value of GPIO"<< Gpio <<" ."<< endl;
        getvalgpio.close();
        return -1;
    }

    getvalgpio >> val ;  //read gpio value

    if (val != "0") {
        ret = PrepareValue(1);
    } else {
        ret = PrepareValue(0);
    }

    CachedValue = ret;

    getvalgpio.close();
    return ret;
}
int TSysfsGpio::InterruptUp() {
    string path="/sys/class/gpio/gpio";
    int _fd;
    string path_to_edge = path + to_string(Gpio) + "/edge";
    string path_to_value=path + to_string(Gpio) + "/value";
    if ( access( path.c_str(), F_OK) == -1 ) return -1;
    ofstream setInterrupt(path.c_str());
    if (!setInterrupt.is_open()){
        cout << " OPERATION FAILED: Unable to set the interrupt of GPIO"<< Gpio <<" ."<< endl;
        setInterrupt.close();
        return -1;
    }
    setInterrupt << "both" << endl;
    InterruptSupport = true;
    _fd=open(path_to_value.c_str(), O_RDONLY | O_NONBLOCK );
    if (_fd <= 0 ) {
        cout << "cannot open value for GPIO" << Gpio <<endl;
    }
    ev_d.data.fd=_fd;
    return 0;
}

 bool TSysfsGpio::getInterruptSupport() {
    return InterruptSupport;
}

int TSysfsGpio::getFileDes() {
    return ev_d.data.fd;
}

struct epoll_event* TSysfsGpio::getEpollStruct() {
    return &ev_d;
}
TSysfsGpio::~TSysfsGpio(){
    //~ if (IsExported()) {
        //~ Unexport();
    //~ }
}
