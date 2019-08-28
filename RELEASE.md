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

