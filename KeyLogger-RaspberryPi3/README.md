# KeyLogger-RaspberryPi3

Summary:
Keylogger on a Linux kernel module, GPIO button silencing/activating the module in run time.
Logged keys saved on txt log file.

Requirements:

OS: Linux (version 4.9 or above), GCC (version 4.9.2).
Raspberry Pi 3.
LED with 2 cables connected to resistor.
Button with 3 cables connected to resistor.


Instructions to Module installation:

1. Download to a folder the files: keylogger.c and Makefile
2. Open terminal
3. Type in the command line: cd /"the path of the folder of the files" (e.g. $ cd /home/pi/Desktop/FolderName).
4. Compile the file by typing in the command line: make (i.e. $ make).
5. Insert the Module to the Kernel by typing in the command line: sudo insmod keylogger.ko (i.e. $ sudo insmod keylogger.ko).

If no errors has occured and everything has been installed into the Kernel, the program is now on idle mode in the system.

[Optional]
In order to check if everything is working as expected, type in the command line: dmesg (i.e. $ dmesg),
if the msg: "Initialization is Complete." appears in the log file, the module is working as expected.

If an error has occured and you see 1 of the following msgs in the dmesg log:
1. Invalid LED/BTN GPIO. (should'nt occure on Raspberry Pi 3).
  - meaning that the number of the GPIO is invalid in your GPIO hardware.
    make sure your hardware GPIO numbers match the code number, change the numbers in the source code if needed.
2. Failed to sign an interrupt.
  - meaning that the request_irq(...) function has failed to assign the interrupt.
    check on the web why might be the cause.
    
Once the Module is in the system on idle mode, you can use the physical button connected to switch the modes.
From idle to listening and vise versa.

The log file might be found in the directory: /home/LogFile.txt  - global directory to all the users.
Can see the content typing in the command line: cat /home/LogFile.txt (i.e. $ cat /home/LogFile.txt).


Instructions to uninstall the Module:

1. Open terminal
2. Type in the command line: sudo rmmod keylogger.ko (i.e. $ sudo rmmod keylogger.ko).
3. [Optional] Type in the command line: sudo rm /home/LogFile.txt (i.e. $ sudo rm /home/LogFile.txt) in order to delet the log file.
4. [Optional] Delete the files from the folder.

Now the system is clear from our module.
