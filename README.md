# ThingyAggregatorIOC
An EPICS IOC for a Bluetooth star network of Nordic thingy:52s and a central NRF52DK aggregator.

Supported sensors:
- Battery
- Button
- Temperature
- Pressure
- Humidity
- Gas / Air Quality
- Quaternions
- Accelerometer
- Gyroscope
- Compass
- Roll, pitch, yaw
- Heading

The IOC also supports control of the Thingy's 4 external digital pins. See [here](https://devzone.nordicsemi.com/nordic/nordic-blog/b/blog/posts/thingy-52-mosfet-usage) for a simple tutorial on using these pins to light an external LED.

## Requirements ##
- Bluetooth Low Energy connectivity
	- Developed with a [Nordic nRF52840 USB dongle](https://www.mouser.com/ProductDetail/Nordic-Semiconductor/nRF52840-Dongle?qs=gTYE2QTfZfTbdrOaMHWEZg%3D%3D&gclid=EAIaIQobChMIlsWN8orC5gIVQ8DICh28-g3LEAQYASABEgJoWfD_BwE) with [Zephyr firmware](https://devzone.nordicsemi.com/f/nordic-q-a/43087/hciconfig-is-not-showing-my-nrf52840-dongle-on-my-linux-terminal)
	- See [here](https://github.com/zephyrproject-rtos/zephyr/issues/11016#issuecomment-447129450) for help setting up the dongle with BlueZ
- At least one [Thingy:52](https://www.nordicsemi.com/?sc_itemid=%7B3C201A33-5CA5-457B-87E4-A7B04C19EE71%7D) with default firmware
- A [NRF52DK](https://www.nordicsemi.com/?sc_itemid=%7BF2C2DBF4-4D5C-4EAD-9F3D-CFD0276B300B%7D) with multi-link aggregator firmware
	- https://github.com/RollandMichael7/nrf52-ble-multi-link-multi-role/
	- Firmware installation requires:
		- [Custom nRF5_SDK_v15.3.0](https://github.com/RollandMichael7/nrf-sdk-v15.3)
		- [Segger Embedded Studio](https://www.segger.com/products/development-tools/embedded-studio/)
		- A micro USB cable
- Software requirements:
  - Bluetooth GATT C library
    - https://github.com/labapart/gattlib
    - Developed with tag ```dev-42-g3dd0ab4```
  - GLib C library
  - EPICS Base

## Setup ##

### Aggregator Firmware ###
To download & setup the SDK + firmware, use the ```build_fw.sh``` script. Next, open Segger Embedded Studio and use ```File > Open Solution...``` to open 
```$(SDK)/examples/training/nrf52-ble-multi-link-multi-role/ble_aggregator/pca10040/ses/ble_aggregator_pca10040_s132.emProject```. Plug the DK into your
computer with the micro USB cable, and in Segger Embedded Studio connect to the DK with ```Target > Connect J-Link```. Compile and flash the firmware 
to your DK with ```Target > Download ble_aggregator_pca10040_s132```. If installation is successful, your DK should automatically begin provisioning
your Thingys and they will light up in a constant light blue as they are connected to the network. On the DK, use button 3 to toggle the LEDs of the
connected Thingys and button 2 to toggle provisioning of nearby Thingys. LED 2 on the DK will blink rapidly when the DK is searching for Thingys to
add to the network. 

### Connection ###
To connect to your Thingy network, the Bluetooth address of the aggregator must be known. There are several Bluetooth command-line tools to do this, as well
as a provided C program. Run ```build.sh``` to compile the program, and it will be installed in the base folder as ```thingy_scan```.
**Note**: At this point ```build.sh``` will probably fail to compile the IOC because it is not setup yet.

If your Bluetooth is set up correctly, the program should give a list of nearby Bluetooth devices with their address and name. The aggregator's name should 
show up as 'Aggregator'. Once you've found your aggregator's address, enter it into ```iocBoot/iocThingy/st.cmd``` as the argument to ```thingyConfig()```. 

**Note:** If the Bluetooth receiver you'd like to use for scanning/connecting isn't your default receiver, you must edit the source files. In 
```ThingyApp/src/thingy_aggregator.c``` find the call to ```gattlib_connect``` in ```get_connection()``` and edit the first argument to match
the HCI index of your desired receiver, eg. ```gattlib_connect('hci1', g_mac_address, GATTLIB_CONNECTION_OPTIONS_LEGACY_BDADDR_LE_PUBLIC | GATTLIB_CONNECTION_OPTIONS_LEGACY_BT_SEC_LOW);```. 
Do the same for ```ThingyApp/src/thingy_name_assign.c``` and ```ThingyApp/src/thingy_scan.c``` if you want to use those tools.
If you don't know the HCI index for your receivers, use the command-line tool ```hciconfig``` to view your devices.

#### Using custom node IDs ####
By default, the node ID of each Thingy is assigned sequentially as they connect to the aggregator. This means that if two Thingy devices
disconnect from the aggregator, their IDs will switch if they reconnect in reverse order. It may be desirable for each Thingy
to be assigned a persistent node ID regardless of the order they connect. This IOC supports this functionality, however it is disabled by default. To enable
it, uncomment the line ```#define USE_CUSTOM_IDS``` in ```ThingyApp/src/thingy_shared.h```. In that file is also the definition of ```CUSTOM_NODE_NAME```.
When ```USE_CUSTOM_IDS``` is defined, the IOC will parse the Bluetooth name of connecting Thingys to see if they match ```CUSTOM_NODE_NAME``` followed by a number
which will be its node ID. For example, if ```CUSTOM_NODE_NAME``` is ```Node``` then a device with name ```Node2``` will be assigned ID 2. A program is provided
for setting a device's Bluetooth name; build the program with ```build.sh``` and it will be installed in the base folder as ```thingy_name_assign```. The tool
takes 2 arguments: the device's Bluetooth address (which can be found with ```thingy_scan```) and the desired name. After defining ```USE_CUSTOM_DS``` and 
```CUSTOM_NODE_NAME``` and setting device names, run the IOC as usual and it will use custom node IDs.

### IOC ###
For each Thingy node in your network, add a line to the ```nodes.substitutions``` file in ```ThingyApp/Db```. Each line in this file will automatically
generate all the PVs for each node, of the form ```{Sys}{Dev}X``` where X is the attribute, and ```Sys``` and ```Dev``` are fields of the substitutions
file. The ```nodeID``` field identifies the Thingy in the network; by default these IDs are assigned sequentially as the nodes connect to the aggregator. Additionally,
set ```Sys``` and ```Dev``` for your aggregator PVs in ```ThingyApp/Db/aggregator.substitutions```.

Edit ```configure/RELEASE``` to point to your installation of EPICS base. Compiling the IOC also requires installing the gattlib C library. 
Use the ```build_gattlib.sh``` script to clone, compile and package gattlib locally, then navigate to ```gattlib/build``` and install the 
appropriate package for your operating system. Alternatively, see the [gattlib repo](https://github.com/labapart/gattlib) for links to prebuilt packages.

**Note:** Future updates to gattlib may break compatibility with the IOC, so to ensure the IOC works correctly use git tag ```dev-42-g3dd0ab4``` or
commit ```5c7ee43bd70ee09a7170ddd55b9fdbdef69e9080```, ```fixed 'apt-install' to 'apt install'```.

## Running the IOC ##

Compile the IOC with ```make``` and run it with the ```st.cmd``` file. View your process variables with the included Control Systems Studio OPI files in
```ThingyApp/op/opi``` after editing the ```Sys``` and ```Dev``` macros to match the ones entered in the substitutions file. ```ThingyApp/op/opi/thingyNetwork.opi```
serves as a "main" page for navigating all of the functionality of the IOC.
