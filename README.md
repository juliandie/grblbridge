# GRBL headless bridge

Important notification about security
There's no drop in privileges. If you use this as root, there might be a major risk for your system if there's unknown attendies in your network.
I just run this bridge as is. I didn't verify nor check for any security related issues.

Use this software on your RapberryPi or Notebook, which is conencted to your device and your network via LAN or WIFI
Personally I use Lightburn to connect to the brdige via GRBL (LAN) to my laser.

Basically this is just a bridge from a TCP-Socket to UART.

# Build
```bash
make
```

# Usage
```bash
./grblbridge [-hv] [-p port] [-m port] <serial-port>
```

## Required arguments

serial-port The device-node of your device (e.g. /dev/ttyUSB0).
The application tries to connect to the serial-port (device) dynamically.
This means, if the device isn't turned on, therefore the device-node isn't available yet, the application won't complain.
As the serial-port appeared the application will open and configure it during runtime.

## Optional arguments
-h Show available arguments


-v Be verbose


-p GRBL port. Default port is 23 (telnet). Since port 23 is < 1024 root-privileges are required.
Keep in mind, the application does not drop privilidges (yet) as the port was opened.


-m Monitoring port. This port is disabled, when no port is given.
Set a monitoring port to see in a separate terminal (e.g. netcat) what messages are sent between the GRBL application and the device.

# Monitor

Connect to monitoring interface
```bash
nc <grblbridge-ipv4> <monitoring-port>
```
