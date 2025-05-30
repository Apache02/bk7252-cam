# encoding: utf8
#
# UART Download Tool
#
# Copyright (c) BekenCorp. (chunjian.tian@bekencorp.com).  All rights reserved.
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import time
from .boot_intf import CBootIntf

try:
    from serial import Timeout
except ImportError:
    from serial.serialutil import Timeout
import binascii
from tqdm import tqdm
from .flash_list import *
from .crc32v2 import *

debug = False


class UartFlashReader(object):
    def __init__(self, port='/dev/ttyUSB0', baudrate=115200):
        self.bootItf = CBootIntf(port, 115200, 0.001)
        self.target_baudrate = baudrate
        self.pbar = None

    def log(self, text):
        """
        print text to tqdm progress bar
        """
        if not self.pbar:
            print("{}".format(text))
        else:
            self.pbar.set_description("{:<16}".format(text))

    def do_reset_signal(self):
        self.bootItf.ser.dtr = 0
        self.bootItf.ser.rts = 1
        time.sleep(0.2)
        self.bootItf.ser.rts = 0

    def readflash(self, startAddr, readLength):
        self.pbar = tqdm(range(readLength // 0x1000), ascii=True, ncols=80, unit_scale=True,
                         unit='k', bar_format='{desc}|{bar}|[{rate_fmt:>8}]')

        self.do_reset_signal()
        self.log("Getting bus...")
        timeout = Timeout(10)

        # Step2: Link Check
        count = 0
        # Send reboot via bkreg
        self.bootItf.SendBkRegReboot()
        while True:
            r = self.bootItf.LinkCheck()
            if r:
                break
            if timeout.expired():
                self.pbar.close()
                self.log('Cannot get bus.')
                return b''

            count += 1
            if count > 0 and count % 100 == 0:
                # Send reboot via bkreg
                self.bootItf.SendBkRegReboot()

                if (count // 100) & 1:
                    self.bootItf.ser.baudrate = 921600
                    self.log("try 921600")
                else:
                    self.bootItf.ser.baudrate = 115200
                    self.log("try 115200")

                # Send reboot via command line
                self.bootItf.Start_Cmd(b"\n")
                self.bootItf.Start_Cmd(b"reboot\n")

                # reset to bootrom baudrate
                if self.bootItf.ser.baudrate != 115200:
                    self.bootItf.ser.baudrate = 115200

        self.log("Gotten Bus...")
        time.sleep(0.01)
        self.bootItf.Drain()

        # Step3: set baudrate, delay 100ms
        if self.target_baudrate != 115200:
            if not self.bootItf.SetBR(self.target_baudrate, 20):
                self.log("Set baudrate failed")
                self.pbar.close()
                return b''
            self.log("Set baudrate successful")

        addr = startAddr & 0xfffff000
        count = 0
        buffer = bytes()

        while count < readLength:
            sector = self.bootItf.ReadSector(addr)
            if sector is not None:
                buffer += sector
            addr += 0x1000
            count += 0x1000
            self.pbar.update()

        self.pbar.close()
        self.pbar = None

        self.log("Check CRC: ")
        ret, flash_crc = self.bootItf.ReadCRC(startAddr, startAddr + count, 5)
        print("   flash: {:x}".format(flash_crc))

        buffer_crc = crc32_ver2(0xffffffff, buffer)
        print("  buffer: {:x}".format(flash_crc))

        if buffer_crc == flash_crc:
            self.log("Read Successful")
        else:
            self.log("CRC not equal")

        for i in range(3):
            time.sleep(0.01)
            self.bootItf.SendReboot()

        return buffer[:readLength]


if __name__ == '__main__':
    reader = UartFlashReader()
