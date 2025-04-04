uSEQ I2C Notes on adding expander or connecting two uSEQ:


Installing/compiling:
1) download the 'i2c' branch repo
Either..
2) there are precompiled firmwares in /uSEQ/firmwareVersions to drag and drop
or
2) you need to set lines 11 and 12 in configure.h to match the hardware

#define USEQHARDWARE_1_0  
//#define USEQHARDWARE_EXPANDER_OUT_0_1

    ... upload. :)

3) Connect a two pin jumper between your modules - note the orientation of SDA and SCL


Use:
From uSEQ perform..

1) Start the module in host mode

(i2c-host-start)

This will print a load of debug data in the serial port window.  As long as this doesn't say "no devices found" this can be ignored - Currently the names and addresses are not used. But are accessed in i2c address order with "1" being the lowest.  So if you have several devices then you will have to experiment to identify which is which.

e.g.

(send-to 1 (d1 1))

(send-to (LISPcode))

Currently an implicit 'do' gets added to the (LISPcode)

so 

(send-to 1 ( (define u.ain1 0.5) (define u.ain2 0.6) ) )

gets sent as

@(do ( (define u.ain1 0.5) (define u.ain2 0.6) ) )

- we can use this a way of sharing input data