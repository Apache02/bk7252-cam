# BK7252 camera

Description here...

## Requirements

* gcc-arm-none-eabi
* cmake
* make
* python3
* any serial monitor (tio for example)

### Install requirements on Ubuntu
```shell script
sudo apt install cmake build-essential gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
sudo apt install python3 python3-hid python3-serial python3-tqdm
```


## Build

```shell script
mkdir build
(cd build && cmake .. && make freertos_shell)
```


## Backup

```shell script
./backup.sh
```


## Flash

```shell
./flash.sh
```


## UART Monitor

```shell
tio -b 115200 /dev/ttyUSB0
```

## Links

...