Github repository

https://github.com/elyomtz/nxp_simtemp


Links to the videos:

Video 1, using simulated temperatures

https://github.com/elyomtz/nxp_simtemp/blob/main/media/video%201.%20Simulated%20temperature.mp4


Video 2, using a Raspberry Pi

https://github.com/elyomtz/nxp_simtemp/blob/main/media/video%202.%20Real%20temperature.mp4

(Use "View raw" to download and watch the videos)



### Build/Run Steps

1. Clone the repository

2. Go to the folder

nxp_simtemp/scripts

3. Run the file build_sim.sh

![Scripts](https://github.com/elyomtz/nxp_simtemp/blob/main/media/r_image1.png)


4. Navigate to the path nxp_simtemp/build/sim and change the privileges to root admin (sudo su)

![Folder](https://github.com/elyomtz/nxp_simtemp/blob/main/media/r_image2.png)

If you want to know the available commands for the CLI, execute simtemp --help

5. Basic execution

You have to run these commands to check the basic functionality
- Load the driver with this command

**simtemp load**

- To start measuring, execute the command:

**simtemp run**

- After some seconds, stop the execution using Ctrl+C

- To unload the driver execute this command: 

**simtemp unload**

![Example](https://github.com/elyomtz/nxp_simtemp/blob/main/media/r_image3.png)

For more details on the system functionality, and an example of execution, please go to the design document (DESIGN.md) and check the sub-section “Executing with simulated temperatures”.
