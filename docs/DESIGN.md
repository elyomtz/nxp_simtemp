## Block Diagram and Interactions

![BlockDiagram](https://github.com/elyomtz/nxp_simtemp/blob/main/media/image1.png)

### Interactions

At the user space, using the CLI, the first command that needs to be sent is “load”. This action sends to the kernel the commands to load the device tree overlay and insert the .ko module. (The device tree overlay is loaded only if a physical device is involved).

When the module is inserted, it creates the device, and assigns the sysfs class functions to it.

Also at the insertion of the module, the probe function is in charge of reading the values that are listed in the device tree overlay.

If the temperature needs to be read from an I2C sensor, it is added using the function _i2c_add_driver_, which uses the characteristics from the device tree binding. If the temperature is simulated, the timer is declared at this stage.

When the user sends the command “run” from the CLI, the app calls the _write_ function, in its counterpart in the kernel the function _f_ops_write_ is called, which starts the threads and the timer (necessary for simulated temperature values). This command also starts the polling process in the app, which will wait for an r-event (POLLIN).

**Threads**

There are two threads that execute on the module.

One of the threads contains a wait queue that checks for a change of state and it also checks whether a timeout has expired or not. A change of state indicates that an attribute of the sysfs class has changed, this could be a request for a different sampling rate, or changing the limit for thresholds (for high o low temperature). An expiration of the time indicates to the poll function that a temperature value needs to be read from user space (POLLIN event).

The second thread keeps on calling the function _measure_and_compare_ (locked by a mutex), with the purpose of checking if a limit has been reached or passed (for low or high temperature), if one of those events has occurred, it indicates to the poll function that data needs to be read from user space.

In the user space, when an r-event POLLIN is available, the app calls its _read_ function, then calling the _f_ops_read_ function in the kernel module, which is in charge of calling the function _measure_and_compare_ (locked by a mutex), and it also sends the data to user space with the function _copy_to_user_.

The mutex lock has been chosen for the call to the function _measure_and_compare_ after the first approach (spinlock) because it has been seen that there was a noticeably delay when it had been called.

What the function _measure_and_compare_ does, is getting the temperature (from the I2C sensor or the timer), then it compares this value with the limits defined in the device tree or those that have been sent by sysfs from the CLI. (If the temperature is simulated, those limits are initially hardcoded when variables are defined). This function also reads the value of the current time. The information obtained in this function will be used to fill this structure:

![Struct](https://github.com/elyomtz/nxp_simtemp/blob/main/media/image2.png)

This function also stores data when a limit has been passed,  because this information is retrieved by the user app when sysfs attribute _stats_ is called.

**sysfs interaction**

When some commands like _simtemp sampling 1000_ or _simtemp_ _htemp 25000_ are sent using the CLI, the kernel module receives them using the sysfs store functions declared in code (like _sysfs_sampling_store_ or _sysfs_htemp_store_). The values received are then used to change the timeout for the wait queue that is executed on the first thread, or are stored to be used as the values against which to make a comparison of the limits for high or low temperature.

When the CLI send commands like _simtemp g_mode_ or _simtemp stats_, the kernel modules processes them with the show functions (_sysfs_mode_show_ and _sysfs_stats_show_ in this case) to sent this information to the user space.


## DT mapping

For the physical device I am using, I have a device tree overlay, here it is the description

![DeviceTree](https://github.com/elyomtz/nxp_simtemp/blob/main/media/image3.png)


## Script files

Inside the script folder there are 4 files.

![Scripts](https://github.com/elyomtz/nxp_simtemp/blob/main/media/image4.png)

This is how they work:

**build_sim.h**

This file is used when you want to get simulated temperature values, so it can be used in a Linux distribution like Ubuntu with the kernel headers installed.

Execute it and it will create a folder in this path: simtemp/build/sim, inside the folder you could see the kernel module (ko file) and the CLI app.

![build_sim](https://github.com/elyomtz/nxp_simtemp/blob/main/media/image5.png)

**build_real.sh**

This script is the one I was using to create the necessary files for executing the code on a Raspberry Pi, it generates the folder simtemp/build/real, with device tree overlay, kernel module and CLI.

![build_real](https://github.com/elyomtz/nxp_simtemp/blob/main/media/image6.png)

**build_demo.sh**

It is mandatory to execute this script before executing the file run_demo.sh, because it creates the necessary files to execute the demo.

**run_demo.sh**

This file runs a small demonstration of the system.

## Compilation and execution

### Executing with simulated temperatures

1. **Starting**

First you need to execute the script build_sim.sh.
After that, it is recommended to open two Terminals, navigate to the path where the files are generated (simtemp/build/sim) and use root privileges in both windows.

To get to know which commands are available in the CLI, you can execute

**simtemp --help**

![sim1](https://github.com/elyomtz/nxp_simtemp/blob/main/media/image7.png)

2. **Loading the driver**

Execute the command

**simtemp load** 

![sim2](https://github.com/elyomtz/nxp_simtemp/blob/main/media/image8.png)

 The linux kernel module is loaded, and on the screen the message “Driver loaded” is displayed.
 
3. **Measuring temperature**

(In this case, the temperature is obtained from random values that are generated inside a timer callback).

Execute the command

**simtemp run**

The system starts reading temperature values each one second:

![sim3](https://github.com/elyomtz/nxp_simtemp/blob/main/media/image9.png)

4. **Changing parameters**

For changing the sampling time, you use this command:

**simtemp sampling 500**

![sim4](https://github.com/elyomtz/nxp_simtemp/blob/main/media/image10.png)


If you want to change the limit for high temperature alert, change it using the command (In this example the alert will be 30°C).

**simtemp htemp 30000**

![sim5](https://github.com/elyomtz/nxp_simtemp/blob/main/media/image11.png)

 (Value in the parameter is given in millidegrees Celsius)
 
The same happens for the low temperature alert, change the value using the command

**simtemp ltemp 15000**
 
![sim6](https://github.com/elyomtz/nxp_simtemp/blob/main/media/image12.png)

Every time the temperature goes beyond one of those limits, it sets the corresponding bit to 1, for low or high temperature:

![sim7](https://github.com/elyomtz/nxp_simtemp/blob/main/media/image13.png)


Even when the system has been set to a high sampling time, 30 seconds, for example, it detects when temperature goes beyond the threshold and displays the alert (wakes between those long periods).

The functions to set or get the *mode* have no implementation, but it is possible to use them to write or read a value using the sysfs variable, for example:

**simtemp s_mode ramp**

**simtemp g_mode**

![sim8](https://github.com/elyomtz/nxp_simtemp/blob/main/media/image14.png)


The stats command only shows the value of the last time an error occurred:

**simtemp stats**

![sim9](https://github.com/elyomtz/nxp_simtemp/blob/main/media/image15.png)

5. **Stop execution**

To stop measuring it is necessary to execute Ctrl+C on the Terminal that is showing the temperature values (more about this topic on the *Challenges* section).

6. **Unloading the driver**

Execute the command 

**simtemp unload**

![sim10](https://github.com/elyomtz/nxp_simtemp/blob/main/media/image16.png)




### Executing demo

It is necessary to execute first the script **build_demo.sh**, after that, it is recommended to get root privileges (sudo su) to avoid the system prompting for password.

Then, execute the script **run_demo.sh**

It will show the output from some of the CLI commands

- Show help
- Load the driver
- Configure sampling to 1000 ms
- Configure low temperature alert
- Acquire temperature values. It will take 30 samples (if there is an alert it will last less than 30 seconds because an interruption due to trespassing a limit will wake the system in between samples).
- Show stats (from CLI command)
- Unload the driver

![demo](https://github.com/elyomtz/nxp_simtemp/blob/main/media/image17.png)




### Executing on a Raspberry Pi

In this case, I have used a Raspberry Pi 3B+, and I have connected a temperature sensor TC74 to the channel 1 of the I2C module.

![real1](https://github.com/elyomtz/nxp_simtemp/blob/main/media/image18.png)


I have run the script ./build_real.sh, generating the folder simtemp/build/real

After opening that folder in a Terminal I have executed the commands:

simtemp load

simtemp run

simtemp stats

simtemp unload

![real1](https://github.com/elyomtz/nxp_simtemp/blob/main/media/image19.png)


As it can be seen, the results are similar to those obtained when executing the system with simulated temperatures, but in this case the decimal position for the measurement is always 0, because the TC74 sensor has only an eight-bit output.



### Challenges

On this section I want  to mention some of the difficulties I have been facing or what I would do with more time.

- Stop polling from an external signal.

Initially, I want to have one command  on the CLI to stop the execution of the running action, but I have seen that I needed to use a signal handler or use a pipe with two files descriptors.
I couldn't make it work either of those methods and I was spending much time on it, so I decided to stop the execution of the poll using Ctrl+C.
In the case of the run_demo.sh script, it needs their dedicated files because the approach was to exit the poll based on a counter.
With more time I would like to investigate more about this topic.

- I couldn't complete the high level test plan, but for T5, I have noticed that when I execute the calls to the sysfs functions the wait queue is awakened, the remaining time of the event doesn't complete its cycle, so it is starts again after reading the value of sysfs attribute, causing a delay. I hadn't time to check how could I solve it.

- I would also like to have more time to implement the GUI.

