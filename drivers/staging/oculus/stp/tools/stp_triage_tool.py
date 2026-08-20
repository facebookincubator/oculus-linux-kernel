#!/usr/bin/env python3

import argparse
import os
import sys
import subprocess
from datetime import datetime, timedelta
import logging
from logging.handlers import RotatingFileHandler

filelog = logging.getLogger("filelogger")
fileHandler = RotatingFileHandler(
    f"stp_triage_{datetime.now().strftime('%m-%d-%H_%M')}.log"
)
filelog.addHandler(fileHandler)

NUM_OF_CHANNELS = 32

class TriageUtils:
    def __init__(self):
        pass

    def insert_seperation_line(self, add_eol=False):
        if (add_eol):
            print("====================================================\n")
            filelog.error("====================================================\n")
        else:
            print("====================================================")
            filelog.info("====================================================")

    def check_device_present(self):
        subprocess.run("adb wait-for-device", shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5, check=True)

    def get_device_root(self):
        subprocess.run("adb root", shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5, check=True)

    def prep_device(self):
        self.check_device_present()
        self.get_device_root()

    def run_command(self, prompt: str, command):
        self.insert_seperation_line()
        print(prompt)
        filelog.info(prompt)
        p = subprocess.run(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5, check=True)
        for line in p.stdout.decode("utf-8").splitlines():
            print(line)
            filelog.info(line)
        print("Done!")
        filelog.info("Done!")
        self.insert_seperation_line(add_eol=True)

    def enable_logging(self):
        prompt = "Set kernel console level to enable all logging"
        command = 'adb shell "echo 8 > /proc/sys/kernel/printk"'
        self.run_command(prompt, command)

    def get_fingerprint(self):
        prompt = "Get SoC build fingerprint"
        command = 'adb shell "getprop | grep ro.build.fingerprint"'
        self.run_command(prompt, command)

    def get_dmesg(self):
        prompt = "Dump kernel dmesg"
        command = 'adb shell "dmesg"'
        self.run_command(prompt, command)

    def get_connect_state(self):
        prompt = "Get STP connection state"
        command = 'adb shell "cat /sys/bus/spi/devices/spi0.0/stp_connection_state"'
        self.run_command(prompt, command)

    def get_driver_stats(self):
        prompt = "Get STP driver status"
        command = 'adb shell "cat /sys/bus/spi/devices/spi0.0/stp_driver_stats"'
        self.run_command(prompt, command)

    def get_channel_stats(self):
        for i in range(NUM_OF_CHANNELS):
            prompt = f'Get channel stats for channel number {i}'
            command = f'adb shell "cat /sys/bus/spi/devices/spi0.0/stp/stp{i}/stp_stats"'
            self.run_command(prompt, command)

    def dump_all(self):
        self.get_fingerprint()
        self.get_connect_state()
        self.get_driver_stats()
        self.get_channel_stats()
        self.get_dmesg()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="STP triage tool")
    parser.add_argument("--enable_logging", action="store_true", help="Set kernel console logging level to print all")
    parser.add_argument("--get_fingerprint", action="store_true", help="Get fingerprint of the loaded SoC build")
    parser.add_argument("--get_dmesg", action="store_true", help="Get kernel dmesg")
    parser.add_argument("--get_connect_state", action="store_true", help="Get connection state of the stp driver")
    parser.add_argument("--get_driver_stats", action="store_true", help="Get driver status")
    parser.add_argument("--get_channel_stats", action="store_true", help="Get channel(device) status")

    args = parser.parse_args()

    filelog.setLevel('DEBUG')

    utils = TriageUtils()
    utils.prep_device()

    if not len(sys.argv) > 1:
        utils.dump_all()
        quit()

    if args.enable_logging:
        utils.enable_logging()
    elif args.get_fingerprint:
        utils.get_fingerprint()
    elif args.get_dmesg:
        utils.get_dmesg()
    elif args.get_connect_state:
        utils.get_connect_state()
    elif args.get_driver_stats:
        utils.get_driver_stats()
    elif args.get_channel_stats:
        utils.get_channel_stats()
