#!../../bin/linux-x86_64/thingy

< envPaths


## Register all support components
dbLoadDatabase "$(TOP)/dbd/thingy.dbd"
thingy_registerRecordDeviceDriver pdbbase

thingyConfig("EB:72:8D:20:21:1A")

## Load record instances
dbLoadRecords "$(TOP)/db/aggregator.db"
dbLoadRecords "$(TOP)/db/nodes.db"

iocInit
