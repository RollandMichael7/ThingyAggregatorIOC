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
- At least one thingy:52 with default firmware
- A NRF52DK with multi-link aggregator firmware
	- https://github.com/RollandMichael7/nrf52-ble-multi-link-multi-role/tree/master/ble_aggregator
- Software requirements:
  - Bluetooth GATT C library
    - https://github.com/labapart/gattlib
  - GLib C library
  - EPICS Base
