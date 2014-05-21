#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include "sysfs_gpio.h"

using namespace std;

TSysfsGpio::TSysfsGpio(int gpio, bool inverted)
    : Gpio(gpio)
    , Inverted(inverted)
    , Exported(false)
    , CachedValue(-1)
{

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

TSysfsGpio::~TSysfsGpio(){
    //~ if (IsExported()) {
        //~ Unexport();
    //~ }
}
