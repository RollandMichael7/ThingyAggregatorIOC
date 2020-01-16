#!/bin/sh

echo Cloning SDK...
git clone https://github.com/RollandMichael7/nrf-sdk-v15.3
echo Done.
echo

cd nrf-sdk-v15.3/examples
mkdir training
cd training

echo Cloning aggregator firmware...
git clone https://github.com/RollandMichael7/nrf52-ble-multi-link-multi-role/
echo Done.
