B5L Sample Code (C++)



0. Introduction
This Sample Code communicates with B5L series, OMRON's ToF sensor, to check / set the parameters and get depth images.
There are 2 modes, "Interactive Mode" and "Non-Interactive Mode".
In Non-Interactive Mode, it will automatically measure under the specified conditions.
This Sample Code is written in C ++ / C language and has been confirmed to work on Linux (Raspberry Pi OS Buster, Ubuntu 20.04) and Windows10.


1. File description & How to build
The following files are included in the Sample Code package.

   build.sh             [   Linux   ] Shell script for compilation
   connect_tof.sh       [   Linux   ] Shell script for connection B5L (Use when ToF_Sample execution does not work)
   DetectionCuboid.pdf  [ Win/Linux ] Explanatory drawing of Detection Cuboid setting
   init_gpio7.sh        [RaspberryPi] Shell script for setting GPIO #7 pin as output
   ToF_Sample.cpp       [ Win/Linux ] Sample Code Main
   ToF_Sample.prm       [ Win/Linux ] Parameter File
   ToF_Sample.sln       [  Windows  ] Solution file for Visual Studio 2019
   ToF_Sample.vcxproj   [  Windows  ] Project file for Visual Studio 2019
   TOFApiZ.cpp          [ Win/Linux ] Command Communication code to B5L
   TOFApiZ.h            [ Win/Linux ] Command Communication code header to B5L
   uart.h               [ Win/Linux ] Serial Communication code header
   uart_linux.c         [   Linux   ] Serial Communication code
   uart_windows.c       [  Windows  ] Serial Communication code

Linux:    Run "Build.sh". The executable file "ToF_Sample" is created.
Windows:  Open "ToF_Sample.sln" with Visual Studio 2019. You can build the executable file.


2. Interactive Mode
In Interactive Mode, you can manually issue B5L commands one by one to check the operation.
When you select a command, any command with no parameters will be executed immediately.
If it is accompanied by result output, you will be asked for the name of the file to store, and if you specify this, it will be executed. If not specified, execution will be stopped.
For the command to change the parameter, first it displays the current setting and asks for the value to change. Enter a value to change it, otherwise it will be aborted.


3. Non-Interactive Mode
In Non-Interactive Mode, the distance is measured for the number of frames specified by the setting value in the parameter file (or until it is stopped by Ctrl + C), and the result is output to the file.
If a Detection Cuboid is specified, the specified command will be executed when the conditions are met (see 6-5. Detection Cuboid function).
This function is based on the image of obstacle detection when mounted on a mobile robot.


4. Parameter File
In Non-Interactive Mode, you need to specify the parameter file as an argument.
B5L is initialized according to this content.
It also sets the behavior of Sample Code, such as specifying the output folder.
Please refer to the comments in the Parameter File and the User's Manual of B5L for the contents of the parameters.
The parameter is set to B5L each time it's specified, so if the same parameter is specified more than once, the one specified later will be valid.
Parameters that are not specified will not be updated. If the output folder is not specified, the file will not be output. If you want to output to the current directory, specify ".".


5. About sample code original processing
This Sample Code basically aims to make B5L functions available from C ++ / C as they are, but there are some additional functions.

5-1. BMP File output
The distance data and amplitude data in Polar Coordinate format are output not only as B5L output but also as converted to BMP file format.
Distance data in Polar Coordinate format is converted into a color BMP image that represents near distances in red and fra distances in blue.
At the same time, the distance data from B5L is output as it is as a .dpt file.
In the case of amplitude data, it is converted to a gray BMP image from 0 to 255.
For both BMP data, Low Amplitude is black. Also Overflow and Satulation are white.


5-2. About conversion of PCD image format (float conversion, xyzi)
In the case of the Cartesian Coordinate format that is the PCD image output, two points have been changed from the B5L standard format to improve usability.
Each coordinate value is converted from a standard 2 bytes Integer to an 8 bytes Float.
Also, in the mode in which Amplitude data is output, it is output in xyzi format instead of xyz format.
In this case, the i-field is the Amplitude data converted to a 1-byte value at the 1m position from B5L sensor based on the distance information.


5-3. RAW Data output
If you define RAW_OUTPUT as 1 in ToF_Sample.cpp, the B5L original binary data will be output as a .raw file in all result formats.
By defining it as 0, the BMP / PCD file will be output as shown above.
In the RAW Data output, the following Mask function and Detection Cuboid function do not work.


5-4. Out-of-field Mask function (Non-Interactive Mode only)
By specifying FOV_LIMITATION=1 in the parameter file, pixels outside the field of view are excluded from the distance data (PCD data) output.
In this case, the format of the output PCD data will be Unorganized Datasets.


5-5. Detection Cuboid function (Non-Interactive Mode only)
By specifying DETECTION_CUBOID in the parameter file, when the PCD file is output in Cartesian Coordinate format, you can execute a command by comparing the number of points contained inside the specified Cuboid with the threshold value.
You can specify up to 10 Cudoids from 0 to 9. If you specify the same number multiple times, the one specified later will be valid.
The Cudoid is specified by two XYZ coordinates and consisting of six planes perpendicular to the X, Y, and Z axes.
If the number of points contained in it is greater than or equal (if the threshold is positive) or less than (if the threshold is negative) to the threshold, the specified command will be executed. If the threshold is 0, the setting has no effect.
When FOV_LIMITATION is enabled, out-of-field pixels are not included in the count.

As described above, if you do not specify the output folder, the image file will not be output.
Please use it when using only this function.


[NOTES ON USAGE]
* This sample code and documentation are copyrighted property of OMRON Corporation
* This sample code does not guarantee proper operation
* This sample code is distributed in the Apache License 2.0.

----
OMRON Corporation
Copyright 2019-2022 OMRON CorporationÅAAll Rights Reserved.
