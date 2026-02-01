![TRS-IO-SSD1306-M1](/images/trs-80MotherboardKeyBoard3.jpg?raw=true "Header")


# TRS-IO with a SSD1306 LCD display for my Model 1
<br>

These are my notes for the build of this version of my TRS-IO<br>
This repo is, like my other repo, based on a snapshot of https://github.com/apuder/TRS-IO commit f2acde6.<br>
That original repo is an amazing piece of hardware/software that provides new feature to my retro TRS-80 model 1<br>
Thanks for making that project hardware design and code avialable for others like me to use on our retro machines.

I’ve built several TRS-IO boards (the earlier versions are on my other GitHub repo), and I’ve now rebuilt them using this updated design.<br>

One challenge I ran into was keeping track of the IP address for each my TRS-IO's.<br>
So, when multiple TRS-IO units are powered on at the same time, it becomes difficult to know which device you’re actually connected to.<br>

To solve that —and mostly just for fun and to learn more— I extended the original design to include a small status display.<br>
This makes it easy to see the assigned IP address and other useful information directly from the TRS-IO itself.<br>

Because this codebase has diverged from the original TRS-IO backported source, I can no longer pull an upstream updates if there are any.<br>
If the original repository is updated in the future, those changes won’t be reflected here unless manually merged.<br>
That said, most current development in this space appears to be shifting toward TRS-IO++, so upstream changes may be minimal going forward.<br>

These notes document the steps I took to configure this new hardware design with my modified software.<br>
They are just notes. They’re primarily for my own reference, but they may also be helpful to others following a similar path.<br>

The new, smaller PCB files that include the display are available in this repo.<br>

The only real difference for the hardware of my board is the part layout <br>
and removing the LED and its resistors to be replaced with a SSD1306 display. <br>
Any TRS-IO could be changed to replace the LED, it just has no mount to hold the display in place, though one can be created. <br>


<table>
  <tr>
	<td><img src="https://github.com/kdcgarber/TRS-IO-SSD1306-M1/blob/main/images/Connected.jpg" width="300" height="300"></td>
	<td><img src="https://github.com/kdcgarber/TRS-IO-SSD1306-M1/blob/main/images/DisplayInfo.gif" width="500" height="300"></td>
  </tr>
</table>



<bre>


## 📂 Content in repository

	.
	├── DebugNotes.txt                       - Notes to setup my PC to view output from the ESP-WROOM-32 usb port
	├── SDcardM1auto.zip                     - SD card files for the SMB share
	├── 3d                                   - STL files to 3D print a case for the new TRS-IO-SSD1306
	├── buildFiles                           - FPGA file
	├── doc                                  - Doc from the original site
	├── examples                             - Code samples form the Orignal Site
	├── images                               - Images for the github pages
	├── kicad-SSD1306                        - Contains the kicad data for the project PCB
	├── src                                  - This is the altered code for the ESP-WROOM-32
	└── src/esp/ESP-WROOOM-32-Deployment     - Compiled Binary ESP files ready for deployment


<br>

## The hardware build and BOM

All of the files for the PCB are in the kicad-SSD1306 directory.<br>
I sent the files to PCBWay to have them created.<br>
Since you pay per square millimeter of board, by placing all the surface mount components on the back sidek, I saved money on the board cost.<br>
In my final builds I opted to mount the FPGA, ESP32, and display directly to the board to decrease the thickness of the board.<br>
I've done both ways, but in the end, since these are dedicated to this, I went with directly soldering them with no sockets.<br>

<table>
  <tr>
    <td><img src="https://github.com/kdcgarber/TRS-IO-SSD1306-M1/blob/main/images/front.jpg" width="200" height="200"></td>
    <td><img src="https://github.com/kdcgarber/TRS-IO-SSD1306-M1/blob/main/images/back.jpg" width="200" height="200"></td>
    <td><img src="https://github.com/kdcgarber/TRS-IO-SSD1306-M1/blob/main/images/TRS-IO-SSd1306-inCase.jpg" width="200" height="200"></td>
  </tr>
  <tr>
    <td><img src="https://github.com/kdcgarber/TRS-IO-SSD1306-M1/blob/main/images/BuiltFront.jpg" width="200" height="200"></td>
    <td><img src="https://github.com/kdcgarber/TRS-IO-SSD1306-M1/blob/main/images/BuiltBack.jpg" width="200" height="200"></td>
  </tr>
</table>


<br>
The BOM shows the parts needed and their layout and placement.<br>

<a href="https://kdcgarber.github.io/githubPages-htmlfiles/TRS-IO-SSD1306%20bom/ibom.html" target="_blank">Parts list.</a>
<br>


## There are two paths to get the code deployed to your ESP-WROOM-32.

1 Build the files from source and deploy.<br>
2 Just use the pre-built files to flash your ESP.<br>

<br>
<br>

## Option 1 - The Software build for the ESP-WROOM-32

These notes for the code build are based on the original notes of my TRS-IO build.<br>
The code in this repo already has my code changes in it, so no to alter any code by hand.<br>
I built this for my enjoyment and to learn and have fun.<br>
These changes may not be the best code they could be, but if anyone wants to use it, they can.<br>

I used the ESP-WROOM-32 esp controller (no PSRAM) - https://www.amazon.com/dp/B0B764963C<br>
And I used the Tang Nano 9K FPGA - https://www.aliexpress.us/item/3256804089255675.html<br>


I did this code build on Ubuntu 24.04, as a non-root account<br>

### Prep - installing required components<br>
&nbsp;&nbsp;&nbsp;&nbsp;sudo apt upgrade<br>
### Packages for Espressif<br>
&nbsp;&nbsp;&nbsp;&nbsp;sudo apt-get install git wget flex bison gperf python3 python3-pip python3-setuptools cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0 python3-virtualenv<br>
###  Packages for TRS-IO<br>
&nbsp;&nbsp;&nbsp;&nbsp;sudo apt-get install z80asm sdcc sdcc-libraries<br>
###  Some fixes for linking libraries that are needed by TRS-IO<br>
&nbsp;&nbsp;&nbsp;&nbsp;sudo mkdir -p /lib/z80/<br>
&nbsp;&nbsp;&nbsp;&nbsp;sudo ln -s /usr/share/sdcc/lib/z80/z80.lib /lib/z80/z80.lib<br>
###  Python3 as default:<br>
&nbsp;&nbsp;&nbsp;&nbsp;sudo apt-get install python3 python3-pip python3-setuptools<br>
&nbsp;&nbsp;&nbsp;&nbsp;sudo ln -s /usr/bin/python3 /usr/bin/python<br>
<br>

###  Install ESP-IDF<br>
cd ~/<br>
mkdir esp<br>
cd esp<br>
git clone -b v4.4.7 --recurse-submodules https://github.com/espressif/esp-idf.git<br>
cd ~/esp/esp-idf<br>
./install.sh esp32<br>

### clone this repo
cd ~/esp<br>
git clone --recurse-submodules https://github.com/kdcgarber/TRS-IO-SSD1306-M1.git<br>

### TRS-IO stuff<br>
.  ~/esp/esp-idf/export.sh<br>

## Compile and Flash the ESP<br>
cd ~/esp/TRS-IO-SSD1306-M1/src/esp<br>
idf.py fullclean<br>
make BOARD=trs-io-m1-v14 build<br>

The build will show many warnings, but if it completes with no errors, the rest should be good.<br>
At the bottom of its output, it lists the command to run. The port needs to be changed to match the correct port.<br>

Sometimes while doing the flash, it fails. If it does, just try it again and it will load.<br><br>
~/esp/esp-idf/components/esptool_py/esptool/esptool.py -p /dev/ttyUSB0  -b 460800 --before default_reset --after hard_reset --chip esp32  write_flash --flash_mode dio --flash_size detect --flash_freq 40m 0x1000 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0x10000 build/trs-io.bin 0x190000 build/html.bin<br>


<br><br><br>




## 💾 Option 2 - Just use the pre-built bin files
<br>
This path is to flash the ESP-WROOM-32 from the provided files without a full code rebuild. <br>
The code does not need to be recompiled unless you are making changes or just want to.
This directory contains 4 files that are the files needed for the ESP: src/esp/ESP-WROOOM-32-Deployment<br>
<br>
• bootloader.bin<br>
• html.bin<br>
• partition-table.bin<br>
• trs-io.bin<br>
<br>

To use these files you'll need to to install ESPTOOL and then deploy the code.<br>
I did my file flash from the files via a windows 11 PC.<br>


From a windows command shell (CMD)<br>
### Check that Python and PIP are installed and install if they are not found. 
python --version<br>
&nbsp;&nbsp;Python 3.12.10<br>

pip --version <br>
&nbsp;&nbsp;pip 25.3 from C:\Users\toddr\AppData\Local\Packages\PythonSoftwareFoundation.Python.3.12_qbz5n2kfra8p0\LocalCache\local-packages\Python312\site-packages\pip (python 3.12)<br>

### This installs the ESPTOOL
pip install esptool<br>

### Add a shortcut to get to the path to use the ESPTOOL
doskey esptool="C:\Users\toddr\AppData\Local\Packages\PythonSoftwareFoundation.Python.3.12_qbz5n2kfra8p0\LocalCache\local-packages\Python312\Scripts\esptool.exe" $*j <br>

ESPTOOL should now work<br>

Just enter "esptool", it should give the default help windows message<br>
Plug your ESP-WROOM-32 in via the usb port to your windows 11 pc.<br>

### Now to find the COM port
With the esp plugged into the laptop in windows 11, "mode" will show the COM it's connected to.<br>
The COM will need to be used on the flash command<br>

mode<br>

Status for device COM5:<br>
Baud:            115200<br>
Parity:          None<br>
Data Bits:       8<br>
Stop Bits:       1<br>
Timeout:         OFF<br>
XON/XOFF:        OFF<br>
CTS handshaking: OFF<br>
DSR handshaking: OFF<br>
DSR sensitivity: OFF<br>
DTR circuit:     OFF<br>
RTS circuit:     OFF<br>
<br>

### Move to your code repo and move to the src/esp/ directory. <br>
cd "C:\Users\toddr\OneDrive\Shared\Todd\GitHub\trsnic-ssd1306\src\esp"<br>

There will be 4 bin files in the directory ESP-WROOOM-32-Deployment which will be flashed to the ESP. <br>
 ### Run this command
(Replace COM5 with your actual COM port number if necessary.) <br>

esptool --chip esp32 --port COM5 --baud 460800 write-flash --flash-mode dio --flash-size detect --flash-freq 40m -z 0x1000 ESP-WROOOM-32-Deployment\bootloader.bin 0x8000 bESP-WROOOM-32-Deployment\partition-table.bin 0x10000 ESP-WROOOM-32-Deployment\trs-io.bin 0x190000 ESP-WROOOM-32-Deployment\html.bin <br>


This should flash the ESP and it will now be ready to use.<br>






# Flash the FPGA

### Install openFPGALoader

apt install \\<br>
  git \\<br>
  gzip \\<br>
  libftdi1-2 \\<br>
  libftdi1-dev \\<br>
  libhidapi-hidraw0 \\<br>
  libhidapi-dev \\<br>
  libudev-dev \\<br>
  zlib1g-dev \\<br>
  cmake \\<br>
  pkg-config \\<br>
  make \\<br>
  g++


git clone https://github.com/trabucayre/openFPGALoader<br>
cd openFPGALoader<br>
mkdir build<br>
cd build<br>
cmake .. <br>
cmake --build .<br>
make install<br>




### The FPGA file is TRS-IO.fs<br>

cd  ~/esp/buildFiles<br>
openFPGALoader -b tangnano9k -f TRS-IO.fs

	EXAMPLE:
	root@thebox:/home/kdcgarber/esp/buildFiles# openFPGALoader -b tangnano9k -f TRS-IO.fs

	empty
	write to flash
	Jtag frequency : requested 6.00MHz   -> real 6.00MHz
	Parse file Parse TRS-IO.fs:
	Done
	DONE
	Jtag frequency : requested 2.50MHz   -> real 2.00MHz
	Erase SRAM DONE
	Erase FLASH DONE
	Erasing FLASH: [==================================================] 100.00%
	Done
	write Flash: [==================================================] 100.00%
	Done
	CRC check: Success



<br>

## All done with the Install

After the install is complete, follow the notes on the TRS-IO site to bring up the trs-io.local access point<br> 
and start the configuration.<br>
The site also has the ROM boot loader for the FreHD to go on the SD card<br>
or the SMB (windows share or for me SAMBA share on my Linux server).<br>



<img src="https://github.com/kdcgarber/TRS-IO-SSD1306-M1/blob/main/images/WebPage.gif" width="200" height="200">
