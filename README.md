# GRBL bridge
Use this software on your RapberryPi or Notebook, which is conencted to your laser and your network via LAN or WIFI. Personally I use Lightburn to connect to the brdige via GRBL (LAN).

Basically this is just a bridge from a TCP-Socket to UART.

# Build
```
make
```

# Usage
```
./grblbridge [-hv] [-p port] <serial-port>
```
-p port is optional and changes the TCP port the bridge is listening to, default is 23 which requires root-privileges.
serial-port is the device-node of your laser (e.g. /dev/ttyUSB0)
