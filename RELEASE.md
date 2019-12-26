EPICS IOC for Bluetooth star network of Nordic thingy:52 nodes and NRF52DK aggregator.

Currently supported sensors:

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


R1-0
=================


R1-0-4 12/26
---
- Handle connect & disconnect events
- Support custom node IDs
	- Enable/disable by defining USE_CUSTOM_IDS in ThingyApp/src/thingy_shared.h
	- Node IDs can be set via the Thingy's Bluetooth name
	- Base name set in ThingyApp/src/thingy_shared.h as CUSTOM_NODE_NAME
- Add tools for Bluetooth scanning & setting Bluetooth name
- Add support for reading & writing external digital pins (MOS1 - MOS4)
	- Example for using pins to light an LED: https://devzone.nordicsemi.com/nordic/nordic-blog/b/blog/posts/thingy-52-mosfet-usage


R1-0-3 9/13
---
- Add screens for motion sensors
- Add support for reading & writing motion sensor config
- Add support for toggling quaternions, accelerometer, gyroscope, compass
- Add documentation to README

R1-0-2 8/28
----
- Add support for motion sensors
	- Quaternions, accelerometer, gyroscope, compass, roll/pitch/yaw, heading
- Add support for toggling active environment sensors
- Add support for reading & writing environment sensor intervals
- Add support for reading & writing connection parameters


R1-0-1 7/31/19
----
- Add watchdog thread to detect disconnected nodes
- Add support for battery, temperature, pressure, air quality (eCO2, TVOC) sensors
- Add RSSI PV
- Add LED toggle
- Add OPI screens

R1-0-0 7/24/19
----
- Support for humidity and button sensors for up to 19 nodes

