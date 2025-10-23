/*****************************************************************************
*  file              main.cpp
*
*  description       CLI for simtemp device driver
*
*  author            Elyoenai Martínez
*
*****************************************************************************/


/****************************************************************************
 * Includes
 ****************************************************************************/
#include <iostream>
#include <string.h>
#include <vector>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <stdint.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <iomanip>
#include <thread>
#include "../../kernel/nxp_simtemp.h"

/****************************************************************************
 * Definitions
 ****************************************************************************/
#define FD_PATH   "/dev/simtemp"
#define LOAD      "sudo insmod nxp_simtemp.ko"
#define UNLOAD    "sudo rmmod nxp_simtemp"
#define LOAD_DTOVERLAY "sudo dtoverlay nxp_simtemp.dtbo"
#define UNLOAD_DTOVERLAY "sudo dtoverlay -r"

using namespace std;

/****************************************************************************
 * Class for CLI functions
 ****************************************************************************/
class Ops{
  
int fd;    

public:

    bool isInteger(const string &s){
	for(char c : s){
	    if(!isdigit(c)){
		return false;
	    }
	}
	return true;
    }

    string format_nanoseconds_to_datetime(long long nanoseconds_since_epoch) {
	chrono::nanoseconds ns_duration(nanoseconds_since_epoch);
	chrono::time_point<chrono::system_clock, chrono::nanoseconds> 
	    time_point_ns(ns_duration);
	time_t tt = chrono::system_clock::to_time_t(
	    chrono::time_point_cast<chrono::system_clock::duration>(time_point_ns)
	);
	tm tm = *localtime(&tt); 
	ostringstream oss;
	oss << put_time(&tm, "%Y-%m-%d %H:%M:%S");
	return oss.str();
    }

    
    void run(void){
        simtemp_sample result;
	struct pollfd pfd;
	float temp_float;
	load_file_descriptor();
	int counter=0;
	char run_buf[1]={'s'};  
	write(fd, run_buf, strlen(run_buf)+1);   
	    
        pfd.fd = fd;
        pfd.events = POLLIN;

	
#ifdef DEMO
	while(counter<30){
	   if(poll(&pfd, 1, -1)==1){	
#else	        
	while (1) {
	   poll(&pfd, 1, -1);	
#endif
	    if(pfd.revents & POLLIN){
		counter++;
		read(fd, &result, sizeof(result));
	    
		temp_float = static_cast<float>(result.temp_mC); 	
		temp_float/=1000;
		std::string sample_time = format_nanoseconds_to_datetime(result.timestamp_ns);
			    
		cout << sample_time 
		<< "   temp=" << fixed << setprecision(1) << temp_float <<"°C"
		<<"   high temp alert="<< result.HIGH_TEMP_ALERT
		<<"   low temp alert="<< result.LOW_TEMP_ALERT<<endl;
	    }
#ifdef DEMO	    
         }	
	    else{
		close(fd);
		exit(1);
		}
#endif	    
	}
	close(fd);
    }

    int send_command(const char *command){
	int status = system(command);
		    
	if(status == -1)
	    return -1;
	else if(WIFEXITED(status) && WEXITSTATUS(status) != 0)
	    return -1;
	else
	    return 0;	
    }

    int load_overlay(){
	return send_command(LOAD_DTOVERLAY);
    }	

    int load_driver(){	
	return send_command(LOAD);
    }

    int load_file_descriptor(){
	fd = open(FD_PATH, O_RDWR);
	    if (fd < 0) {
	    cout << "Error opening the device file" << endl;
	    exit(1);
	}
	return 0;	
    }

    int unload_overlay(){
	return send_command(UNLOAD_DTOVERLAY);
    }

    int unload_driver(){
	return send_command(UNLOAD);
    }

    void set_sampling(int value){
	    string command;
	    cout<<"Setting sampling: " << value << endl;
	    command = "echo "+ std::to_string(value) + " > /sys/class/simtemp_class/simtemp/sysfs_sampling_ms";
	    send_command(command.c_str());
    }

    void set_htemp(int value){
	    string command;
	    cout<<"Setting value for high temperature alert: " << value << endl;
	    command = "echo "+ std::to_string(value) + " > /sys/class/simtemp_class/simtemp/sysfs_htemp_mC";
	    send_command(command.c_str());
    }

    void set_ltemp(int value){
	    string command;
	    cout<<"Setting value for low temperature alert: " << value << endl;
	    command = "echo "+ std::to_string(value) + " > /sys/class/simtemp_class/simtemp/sysfs_ltemp_mC";
	    send_command(command.c_str());
    }

    void set_mode(string value){
	    string command;
	    cout<<"Setting mode: " << value << endl;
	    command = "echo "+ value + " > /sys/class/simtemp_class/simtemp/sysfs_mode";
	    send_command(command.c_str());
    }

    void get_mode(){
	    string command;
	    cout<<"Getting mode: " << endl;
	    command = "cat /sys/class/simtemp_class/simtemp/sysfs_mode";
	    send_command(command.c_str());
    }

    void get_stats(){
	    string command;
	    cout<<"Statistics: " << endl;
	    command = "cat /sys/class/simtemp_class/simtemp/sysfs_stats";
	    send_command(command.c_str());
    }
};


int main(int argc, char* argv[]) {
    Ops ops;
    
    if (argc > 1 && std::string(argv[1]) == "--help") {
        cout << "\n\tsimtemp CLI - NXP\n" << endl;
        cout << "Usage: " << endl;
        cout << "\tsimtemp <command> [argument]\n" << endl;
        cout << "Commands:" << endl;
        cout << "\tload                \tLoad the driver" << endl;
        cout << "\tunload              \tUnload the driver" << endl;
        cout << "\trun                 \tStart reading temperature values" << endl;    
        cout << "\tsampling [argument] \tSet the sampling rate (in milliseconds)" << endl;
        cout << "\thtemp [argument]    \tSet the alert for high temperature (in millidegrees Celsius)" << endl;
        cout << "\tltemp [argument]    \tSet the alert for low temperature (in millidegrees Celsius)" << endl;
        cout << "\ts_mode [argument]     \tSet the mode - normal, noisy or ramp" << endl;
	cout << "\tg_mode [argument]     \tGet the current mode" << endl;
        cout << "\tstats               \tShow statistics\n" << endl;
        cout << "Examples:" << endl;
        cout << "\tsimtemp load" << endl;
        cout << "\tsimtemp sampling 2000" << endl;
        cout << "\tsimtemp htemp 32000\n" << endl;
        cout << "Option:" << endl;
        cout << "\t--help    Display this help message\n" << endl;
    } else if (argc > 1 && std::string(argv[1]) == "load") {
#ifdef REAL	
	ops.load_overlay();
#endif	
        ops.load_driver();
      std::cout << "Driver loaded" << std::endl;
    } else if (argc > 1 && std::string(argv[1]) == "unload") {
        ops.unload_driver();
#ifdef REAL
	ops.unload_overlay();
#endif	
        std::cout << "Driver unloaded" << std::endl;  
    } else if (argc > 1 && std::string(argv[1]) == "run") {
	std::cout << "Reading temperature:" << std::endl;
        ops.run();
    } else if (argc > 1 && std::string(argv[1]) == "sampling" && ops.isInteger(std::string(argv[2]))) {
        ops.set_sampling(atoi(argv[2]));
    } else if (argc > 1 && std::string(argv[1]) == "htemp" && ops.isInteger(std::string(argv[2]))) {
        ops.set_htemp(atoi(argv[2]));
    } else if (argc > 1 && std::string(argv[1]) == "ltemp" && ops.isInteger(std::string(argv[2]))) {
        ops.set_ltemp(atoi(argv[2]));
    } else if (argc > 1 && std::string(argv[1]) == "s_mode" && (std::string(argv[2])=="normal" || std::string(argv[2])=="noisy" || std::string(argv[2])=="ramp")) {
        ops.set_mode(std::string(argv[2]));
    } else if (argc > 1 && std::string(argv[1]) == "g_mode"){
        ops.get_mode();	
    } else if (argc > 1 && std::string(argv[1]) == "stats"){
        ops.get_stats();
    } else {
        std::cout << "Command not found" << std::endl;
    }

    return 0;
}
