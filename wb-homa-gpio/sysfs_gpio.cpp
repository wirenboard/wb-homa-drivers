#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include "sysfs_gpio.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

using namespace std;

TSysfsGpio::TSysfsGpio(int gpio, bool inverted, string interrupt_edge)
    : Gpio(gpio)
    , Inverted(inverted)
    , Exported(false)
    , InterruptSupport(false)
    , CachedValue(-1)
    , FileDes(-1)
    , G_mutex()
    , In(false)
    , Debouncing(true)
    , Interval(0)
    , Counts(0)
    , InterruptEdge(interrupt_edge)
    , FirstTime(true)
{
    Ev_d.events = EPOLLET;

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
    , InterruptSupport(other.InterruptSupport)
    , CachedValue(other.CachedValue)
    , FileDes(-1)
    , g_mutex()
    , in(other.in)
    , interval(other.interval)
    , Counts(other.Counts)
{
    this->ev_d.events= EPOLLET;
    this->ev_d.data.fd=other.ev_d.data.fd;
}*/

TSysfsGpio::TSysfsGpio( TSysfsGpio &&tmp)
    : Gpio(tmp.Gpio)
    , Inverted(tmp.Inverted)
    , Exported(tmp.Exported)
    , InterruptSupport(tmp.InterruptSupport)
    , CachedValue(tmp.CachedValue)
    , FileDes(tmp.FileDes)
    , G_mutex()
    , In(tmp.In)
    , Debouncing(true)
    , Interval(tmp.Interval)
    , Counts(tmp.Counts)
    , InterruptEdge(tmp.InterruptEdge)
    , FirstTime(tmp.FirstTime)
{
    Ev_d.events = EPOLLET;
    Ev_d.data.fd = tmp.Ev_d.data.fd;
    tmp.FileDes = -1;
    tmp.Ev_d.data.fd = -1;
}


int TSysfsGpio::Export()
{
    // Do export
    {
        string export_str = "/sys/class/gpio/export";
        ofstream exportgpio(export_str.c_str());
        if (!exportgpio.is_open()) {
            cerr << " OPERATION FAILED: Unable to export GPIO" << Gpio << " ." << endl;
            return -1;
        }
        exportgpio << Gpio; //write GPIO number to export
        exportgpio << "\n";
        exportgpio.close(); //close export file
    }
    // Open value file
    OpenValueFile();
    Exported = true;
    return 0;
}

int TSysfsGpio::Unexport()
{
    string unexport_str = "/sys/class/gpio/unexport";
    ofstream unexportgpio(unexport_str.c_str()); //Open unexport file
    if (!unexportgpio.is_open()) {
        cerr << " OPERATION FAILED: Unable to unexport GPIO" << Gpio << " ." << endl;
        return -1;
    }
    unexportgpio << Gpio ; //write GPIO number to unexport
    unexportgpio.close(); //close unexport file

    if (FileDes >= 0) {
        close (FileDes) ;
    }
    Exported = false;
    cerr << "unexported " << Gpio << endl;
    return 0;
}

int TSysfsGpio::OpenValueFile()
{
    // close file descriptor if it is open
    if (FileDes >= 0)
        close(FileDes);
    string path_to_value = "/sys/class/gpio/gpio" + to_string(Gpio) + "/value";
    // open file descriptro of value file and keep it in FileDes and ev_data.fd
    int fd = open(path_to_value.c_str(), O_RDWR | O_NONBLOCK);
    if (fd <= 0) {
        cerr << strerror(errno) << endl;
        cerr << "cannot open value for GPIO" << Gpio << endl;
        return -1;
    }
    FileDes = fd;
    Ev_d.data.fd = fd;
    cerr << "gpio value-file " << Gpio << " filedes is " << FileDes << endl;
    return 0;
}

int TSysfsGpio::Reload()
{
    OpenValueFile();
    // Mean that Cached value may be equal to -1
    // If it is so, 0 will be written
    SetDirection(In, (CachedValue == 1 ? 1 : 0));
    return 0;
}


int TSysfsGpio::SetDirection(bool input, bool output_state)
{
    cerr << "DEBUG:: gpio=" << Gpio << " SetDirection() input= " << input << endl;

    string setdir_str = "/sys/class/gpio/gpio";
    setdir_str += to_string(Gpio) + "/direction";

    ofstream setdirgpio(setdir_str.c_str()); // open direction file for gpio
    if (!setdirgpio.is_open()) {
        cerr << " OPERATION FAILED: Unable to set direction of GPIO" << Gpio << " ." << endl;
        setdirgpio.close();
        return -1;
    }

    //write direction to direction file
    In = input;
    if (input)
        setdirgpio << "in";
    else
        setdirgpio << (output_state ? "high" : "low");

    setdirgpio.close(); // close direction file
    return 0;
}

int TSysfsGpio::SetValue(int value)
{
    cerr << "DEBUG:: gpio=" << Gpio << " SetValue()  value= " << value << " pid is " << getpid() <<
         " filedis is " << FileDes << endl;

    int prep_value = PrepareValue(value);

    char buf = '0' + prep_value;
	
	// there is lock_guard inside block
	{
		std::lock_guard<std::mutex> lock(G_mutex);
		if (lseek(FileDes, 0, SEEK_SET) == -1 ) {
			cerr << "lseek returned -1" << endl;
		}
		if (write(FileDes, &buf, sizeof(char)) <= 0 ) {
			//write value to value file, FileDes has been initialized in Export
			cerr << strerror(errno);
			cerr << " OPERATION SetValue FAILED: Unable to set the value of GPIO" << Gpio << " ." <<
				 "filedis is " << FileDes <<  endl;
			//setvalgpio.close();
			return -1;
		}
	}  
    CachedValue = prep_value;
	// Check whether device is connected
	// Error when disconnected is raising only when reading from /value
    if (GetValue() < 0)
        return -1;

    return 0;
}

int TSysfsGpio::GetValue()
{
    lock_guard<mutex> lock(G_mutex);
    char buf = '0';

    if (lseek(FileDes, 0, SEEK_SET) == -1 ) {
        cerr << "lseek returned -1 " << endl;
        return -1;
    }
    if (read(FileDes, &buf, sizeof(char)) <= 0) { //read gpio value
        cerr << " OPERATION GetValue FAILED: Unable to Get value of GPIO" << Gpio << " filedes is " <<
             FileDes << "pid is " << getpid() << "." << endl;
        perror("error is ");
        return -1;
    }
    // CachedValue will be 1 or 0
    CachedValue = PrepareValue(buf - '0');
    return CachedValue;
}

int TSysfsGpio::InterruptUp()
{
    string path = "/sys/class/gpio/gpio";
    string path_to_edge = path + to_string(Gpio) + "/edge";
    string path_to_value = path + to_string(Gpio) + "/value";
    if ( access( path_to_edge.c_str(), F_OK) == -1 )
        return -1;
    if (In) {// if direction is input we will write to edge file
        ofstream setInterrupt(path_to_edge.c_str());
        if (!setInterrupt.is_open()) {
            cerr << " OPERATION FAILED: Unable to set the Interrupt of GPIO" << Gpio << " ." << endl;
            setInterrupt.close();
            return -1;
        }
        setInterrupt << GetInterruptEdge();
        setInterrupt.close();
        InterruptSupport = true;
    } else {
        InterruptSupport = false;
    }
    return 0;
}

bool TSysfsGpio::GetInterruptSupport()
{
    return InterruptSupport;
}

int TSysfsGpio::GetFileDes()
{
    return FileDes;
}

struct epoll_event &TSysfsGpio::GetEpollStruct()
{
    return Ev_d;
}

bool TSysfsGpio::IsDebouncing()
{
    if (Counts == 0) return false;
    std::chrono::steady_clock::time_point time_now = std::chrono::steady_clock::now();
    long long debouncing_interval = std::chrono::duration_cast<std::chrono::microseconds>
                                    (time_now - Previous_Interrupt_Time).count();
    if (debouncing_interval > 1000)
        // if interval of impulses is bigger than 1000 microseconds we consider it is not a debounnce
        Debouncing = false;
    else
        Debouncing = true;
    return Debouncing;
}

bool TSysfsGpio::GetInterval()
{
    if (Counts != 0) {
        std::chrono::steady_clock::time_point time_now = std::chrono::steady_clock::now();
        long long unsigned int measured_interval = std::chrono::duration_cast<std::chrono::microseconds>
                (time_now - Previous_Interrupt_Time).count();
        if (measured_interval < MICROSECONDS_DELAY) return false;
        Interval = measured_interval;
        Counts++;
        Previous_Interrupt_Time = time_now;
    } else {
        Counts = 1;
        Previous_Interrupt_Time = std::chrono::steady_clock::now();
    }
    //cerr << "DEBUG: GPIO:" << Gpio << "interval= " << Interval << "counts= " << Counts << endl;
    return true;
}

vector<TPublishPair> TSysfsGpio::MetaType()
{
    vector<TPublishPair> output_vector;
    output_vector.push_back(make_pair("", "switch"));
    return output_vector;
}

vector<TPublishPair>  TSysfsGpio::GpioPublish()
{
    vector<TPublishPair> output_vector;
    if (FirstTime) {
        FirstTime = false;
    } else
        GetInterval(); // remember interval
    int output_value = GetValue();
    if (output_value >= 0) {
        output_vector.push_back(make_pair("", to_string(output_value))); // output saved value
        //output_vector.push_back(make_pair("/meta/error", "")); // no error
    } else {
        //string error_str = string("Can't read value from gpio #") + to_string(Gpio);
        //cerr << "ERROR: GpioPublish: " << error_str << endl;
        //output_vector.push_back(make_pair("/meta/error", error_str)); // output error
    }
    return output_vector;
}

string TSysfsGpio::GetInterruptEdge()
{
    if (InterruptEdge == "") {
        return "both";
    } else
        return InterruptEdge;
}

void TSysfsGpio::SetInterruptEdge (string s)
{
    InterruptEdge = s;
}

void TSysfsGpio::SetInitialValues(float total)
{
}

TPublishPair TSysfsGpio::CheckTimeInterval()
{
    return make_pair(string(""), string(""));
}

TSysfsGpio::~TSysfsGpio()
{
    if (FileDes >= 0) {
        close(FileDes);
    }
}
