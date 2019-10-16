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

## Requirements ##
- Bluetooth Low Energy connectivity
- At least one [Thingy:52](https://www.nordicsemi.com/?sc_itemid=%7B3C201A33-5CA5-457B-87E4-A7B04C19EE71%7D) with default firmware
- A [NRF52DK](https://www.nordicsemi.com/?sc_itemid=%7BF2C2DBF4-4D5C-4EAD-9F3D-CFD0276B300B%7D) with multi-link aggregator firmware
	- https://github.com/RollandMichael7/nrf52-ble-multi-link-multi-role/tree/master/ble_aggregator
	- Firmware installation requires:
		- [nRF5_SDK_v15.3.0](https://developer.nordicsemi.com/nRF5_SDK/nRF5_SDK_v15.x.x/nRF5_SDK_15.3.0_59ac345.zip)
		- [Segger Embedded Studio](https://www.segger.com/products/development-tools/embedded-studio/)
		- A micro USB cable
- Software requirements:
  - Bluetooth GATT C library
    - https://github.com/labapart/gattlib
  - GLib C library
  - EPICS Base

## Setup ##

### Firmware ###
Download Segger Embedded Studio, the aggregator firmware and the NRF SDK v15.2. Create a folder in your SDK installation ```$(SDK)/examples/training```,
and copy the ```nrf52-ble-multi-link-multi-role``` folder in. Open Segger Embedded Studio, and use ```File > Open Soltion...``` to open 
```$(SDK)/examples/training/nrf52-ble-multi-link-multi-role/ble_aggregator/pca10040/ses/ble_aggregator_pca10040_s132.emProject```. Plug the DK into your
computer with the micro USB cable, and in Segger Embedded Studio connect to the DK with ```Target > Connect J-Link```. Compile and flash the firmware 
to your DK with ```Target > Download ble_aggregator_pca10040_s132```. If installation is successful, your DK should automatically begin provisioning
your Thingys and they will light up in a constant light blue as they are connected to the network. On the DK, use button 3 to toggle the LEDs of the
connected Thingys and button 2 to toggle provisioning of nearby Thingys. LED 2 on the DK will blink rapidly when the DK is searching for Thingys to
add to the network. 

### Connection ###
To connect to your thingy network, the Bluetooth address of the aggregator must be known. There are several Bluetooth command-line tools to do this. Try:

```
$ bluetoothctl
[bluetooth]# scan on
```

```$ hcitool lescan```

If your Bluetooth is set up correctly, either of these commands should give a list of nearby Bluetooth devices with their
address and name. The aggregator's name should show up as 'Aggregator'. Once you've found your aggregator's address, enter it 
into ```iocBoot/iocThingy/st.cmd``` as the argument to ```thingyConfig()```. 

### IOC ###
For each Thingy node in your network, add a line to the ```nodes.substitutions``` file in ```ThingyApp/Db```. Each line in this file will automatically
generate all the PVs for each node, of the form ```{Sys}{Dev}X``` where X is the attribute, and ```Sys``` and ```Dev``` are fields of the substitutions
file. The ```nodeID``` field identifies the Thingy in the network; these IDs are assigned sequentially as the nodes connect to the aggregator. Additionally,
set ```Sys``` and ```Dev``` for your aggregator PVs in ```ThingyApp/Db/aggregator.substitutions```.

Edit ```configure/RELEASE``` to point to your installation of EPICS base. Running the IOC also requires downloading, compiling, packaging and installing
the gattlib C library. See the [gattlib repo](https://github.com/labapart/gattlib) for instructions.

## Running the IOC ##

Compile the IOC with ```make``` and run it with the ```st.cmd``` file. View your process variables with the included Control Systems Studio OPI files in
```ThingyApp/op/opi``` after editing the ```Sys``` and ```Dev``` macros to match the ones entered in the substitutions file.
