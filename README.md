# rkudev references

My rk3399 kernel has rkudev device as `/dev/rkudev*`.

## You can

Make own USB peripheral device with this references.

## rkudevd

A daemon for USB peripheral (bulk) service daemon with rkudev.

## rkudevcli

A client reference with libusb.

### USB VID and PID

- If you want USB PID and VID, change these is availed in 
    1. `/usr/bin/usbdevice`
	1. `/etc/init.d/rkudev`
	    - also check `/ect/init.d/.usb_config` too.

