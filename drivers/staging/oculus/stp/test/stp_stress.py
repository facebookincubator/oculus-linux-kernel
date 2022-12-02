#!/usr/bin/env python3

import argparse
import logging
import os
from datetime import datetime, timedelta
from enum import Enum
from logging.handlers import RotatingFileHandler
from sys import exit

from time import sleep

import serial
import structlog

# Binds logging handler to structlog
structlog.stdlib.recreate_defaults(log_level=logging.NOTSET)

consolelog = logging.getLogger("consolelogger")
consoleHandler = logging.StreamHandler()
consoleHandler.terminator = ""
consolelog.addHandler(consoleHandler)

filelog = logging.getLogger("filelogger")
fileHandler = RotatingFileHandler(
    f"stp_test_{datetime.now().strftime('%m-%d-%H_%M')}.log"
)
filelog.addHandler(fileHandler)

flog = structlog.get_logger("filelogger")
clog = structlog.get_logger("consolelogger")


class EvaluateResult(Enum):
    SYNCED = 0
    RESYNC = 1
    TIMEOUT = 2


class RebootRequiredError(Exception):
    pass


class STPLogger:
    def info(self, msg):
        if "stp" in msg.lower():
            flog.info(msg)

    def warn(self, msg):
        if "stp" in msg.lower():
            flog.warn(msg)

    def error(self, msg):
        if "stp" in msg.lower():
            flog.error(msg)


log = structlog.wrap_logger(STPLogger(), wrapper_class=structlog.BoundLogger)


class EvaluatorInterface:
    def sync(self, timeout: int) -> bool:
        """Returns True if sync occurred"""
        pass

    def mcu_reboot(self):
        pass

    def soc_reboot(self):
        pass

    def mcu_query_build_info(self):
        """Queries build info on mfg image to trigger resync"""
        pass

    def get_log(self) -> str:
        """Returns the log of the last sync session"""
        pass

    def clear_log(self):
        """Clears the cached log"""
        pass


class PluginInterface:
    """
        Tests follow the flow:
                    -------------------------------
                    v                             |
        setup -> start -> stop -> -> ?resync -> ?fail -> close --on quit--> terminate
                    \------\
                            \--on error--> restart
    """

    def setup(self):
        """Called once at test setup"""
        pass

    def terminate(self):
        """Called when plugins are required to terminate for test end"""
        pass

    def restart(self):
        """Called when plugins are required to recover"""
        pass

    def start(self):
        """Called at the beginning of each test iteration"""
        pass

    def stop(self):
        """Called when test evaluation ends"""
        pass

    def resync(self):
        """Called when test required a resync"""
        pass

    def fail(self, filename: str):
        """Called after stop if test fails"""
        pass

    def close(self):
        """Called at the end of each test iteration"""
        pass


class SerialEvaluator(EvaluatorInterface):
    def __init__(self, port: str, baud: int = 115200, timeout: int = 5):
        self.s = serial.Serial(port=port, baudrate=baud, timeout=timeout, exclusive=True)
        self.log = ""

    def sync(self, timeout: int) -> bool:
        tstart = datetime.now()
        tnow = tstart
        while (tnow - tstart) < timedelta(seconds=timeout):
            line = self.s.readline().decode()
            if line:
                self.log += line
            line.strip()
            if "stp_invalidate_session" in line.lower():
                flog.info(line)
                return True
            tnow = datetime.now()
        return False

    def get_su(self):
        self.s.write(b"\n")
        self.s.write(b"su\n")

    def mcu_reboot(self):
        self.get_su()
        self.s.write(b"echo 1 > /sys/devices/platform/soc/soc:meta,rt600_ctrl/reset\n")
        self.s.flush()
        sleep(1)

    def soc_reboot(self):
        self.get_su()
        self.s.write(b"reboot\n")

    def mcu_query_build_info(self):
        self.get_su()
        self.s.write(b"mfg mcu build_info\n")

    def get_log(self) -> str:
        return self.log

    def clear_log(self):
        self.log = ""


class SaleaePlugin(PluginInterface):
    def __init__(self, serial: str, path: str):
        from saleae import automation

        self.serial = serial
        self.path = path
        self.manager = automation.Manager(port=10430)
        self.device_configuration = automation.LogicDeviceConfiguration(
            enabled_digital_channels=[2, 3, 4, 5, 6, 7],
            digital_sample_rate=500_000_000,
            digital_threshold_volts=1.8,
        )

        self.capture_configuration = automation.CaptureConfiguration(
            capture_mode=automation.ManualCaptureMode()
        )

        self.start_program()

    def start_program(self):
        from subprocess import Popen

        from saleae import automation

        os.environ["ENABLE_AUTOMATION"] = "1"
        self.p = Popen(self.path)
        sleep(15)
        self.manager = automation.Manager(port=10430)
        sleep(10)

    def save_capture(self, filename: str):
        from grpc import _channel
        from saleae import automation

        try:
            spi_analyzer = self.capture.add_analyzer(
                "SPI",
                label=f"Test Analyzer",
                settings={
                    "MISO": 5,
                    "MOSI": 6,
                    "Clock": 4,
                    "Enable": 7,
                    "Bits per Transfer": "8 Bits per Transfer (Standard)",
                    "Clock Phase": "Data is Valid on Clock Trailing Edge (CPHA = 1)",
                },
            )

            capture_filepath = os.path.join(os.getcwd(), filename + "_trace.sal")
            self.capture.save_capture(filepath=capture_filepath)
        except automation.DeviceError:
            flog.error("saleae device error")
            self.restart()
            pass
        except (
            automation.MissingDeviceError,
            automation.InternalServerError,
            _channel._InactiveRpcError,
        ) as e:
            print(e)
            flog.error("saleae manager error")
            raise RebootRequiredError

    def setup(self):
        pass

    def terminate(self):
        self.manager.close()
        self.p.terminate()

    def restart(self):
        self.terminate()
        self.start_program()

    def start(self):
        from grpc import _channel
        from saleae import automation

        try:
            self.capture = self.manager.start_capture(
                device_serial_number=self.serial,
                device_configuration=self.device_configuration,
                capture_configuration=self.capture_configuration,
            )
        except automation.DeviceError:
            flog.error("saleae device error")
            self.restart()
            pass
        except (
            automation.MissingDeviceError,
            automation.InternalServerError,
            _channel._InactiveRpcError,
        ):
            flog.error("saleae manager error")
            raise RebootRequiredError

    def stop(self):
        from grpc import _channel
        from saleae import automation

        try:
            self.capture.stop()
        except automation.DeviceError:
            flog.error("saleae device error")
            self.restart()
            pass
        except (
            automation.MissingDeviceError,
            automation.InternalServerError,
            _channel._InactiveRpcError,
        ):
            flog.error("saleae manager error")
            raise RebootRequiredError

    def resync(self, filename: str):
        self.save_capture(filename)

    def fail(self, filename: str):
        self.save_capture(filename)

    def close(self):
        from grpc import _channel
        from saleae import automation

        try:
            self.capture.close()
        except automation.DeviceError:
            flog.error("saleae device error")
            self.restart()
            pass
        except (
            automation.MissingDeviceError,
            automation.InternalServerError,
            _channel._InactiveRpcError,
        ):
            flog.error("saleae manager error")
            raise RebootRequiredError


class TestPlugins:
    def __init__(self, evaluator: EvaluatorInterface, plugins: list[PluginInterface]):
        self.plugins = plugins
        for p in self.plugins:
            p.setup()

    def __enter__(self):
        for p in self.plugins:
            p.start()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        for p in self.plugins:
            p.close()
            if exc_type is KeyboardInterrupt:
                p.terminate()

    def on_stop(self):
        self.filename = f"stp_{datetime.now().strftime('%m-%d-%H_%M')}"
        for p in self.plugins:
            p.stop()

    def on_fail(self):
        flog.error(self.filename)

        for p in self.plugins:
            p.fail(self.filename)

        with open(self.filename + "_serial.log", "w+") as f:
            f.write(evaluator.get_log())
            evaluator.clear_log()

    def on_resync(self):
        flog.warn(self.filename)

        for p in self.plugins:
            p.resync(self.filename)


class KasaManager:
    def __init__(self, ip: str):
        from asyncio import new_event_loop, set_event_loop

        from kasa import SmartStrip

        self.loop = new_event_loop()
        set_event_loop(self.loop)

        self.strip = SmartStrip(ip)
        result = self.loop.run_until_complete(self.strip.update())

    def power_cycle(self):
        from asyncio import set_event_loop

        from kasa import SmartStrip

        result = self.loop.run_until_complete(self.strip.children[0].turn_off())
        sleep(5)
        result = self.loop.run_until_complete(self.strip.children[0].turn_on())
        sleep(5)


class STPStressTest:
    def __init__(self, evaluator: SerialEvaluator):
        self.evaluator = evaluator
        self.sync_count = 0
        self.timeout_count = 0
        self.resync_count = 0

        self.iterations = 0
        self.consecutive_fails = 0
        self.resyncing = False

    def log(self):
        log.info(
            "stp test stats",
            iterations=self.iterations,
            syncs=self.sync_count,
            timeouts=self.timeout_count,
            resyncs=self.resync_count,
        )

    def evaluate(self) -> EvaluateResult:
        self.iterations += 1
        self.evaluator.clear_log()
        try:
            if self.evaluator.sync(20):
                self.sync_count += 1
                return EvaluateResult.SYNCED
            self.resync_count += 1
            self.evaluator.mcu_query_build_info()
            if self.evaluator.sync(10):
                return EvaluateResult.RESYNC
            self.timeout_count += 1
            return EvaluateResult.TIMEOUT
        except KeyboardInterrupt:
            log.info(
                "stp test stats",
                iterations=self.iterations,
                syncs=self.sync_count,
                timeouts=self.timeout_count,
                resyncs=self.resync_count,
            )
            raise KeyboardInterrupt
        except:
            return False


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="STP stress test")
    parser.add_argument("--serial", type=str, required=True, help="serial port")
    parser.add_argument("--kasa_ip", type=str, help="ip address for kasa smart plug")
    parser.add_argument(
        "--saleae_serial", type=str, help="serial number for saleae analyzer"
    )
    parser.add_argument(
        "--saleae_path", type=str, help="executable path for saleae program"
    )
    parser.add_argument(
        "--exit_on_error", action="store_true", help="halts the test on first failure"
    )

    args = parser.parse_args()

    # Check for all saleae inputs
    if args.saleae_serial and (args.saleae_path is None):
        parser.error("--saleae_serial requires --saleae_path.")
    if args.saleae_path and (args.saleae_serial is None):
        parser.error("--saleae_path requires --saleae_serial.")

    plugin_list = []

    evaluator = SerialEvaluator(port=args.serial)
    test = STPStressTest(evaluator=evaluator)

    kasa = KasaManager(args.kasa_ip) if args.kasa_ip else None

    evaluator.mcu_reboot()
    evaluator.soc_reboot()

    if args.saleae_path:
        plugin_list.append(
            SaleaePlugin(
                serial=args.saleae_serial,
                path=args.saleae_path,
            )
        )

    fail_count = 0
    while True:
        try:
            with TestPlugins(evaluator=evaluator, plugins=plugin_list) as plugins:
                result = test.evaluate()
                plugins.on_stop()

                if result is EvaluateResult.SYNCED:
                    fail_count = 0
                elif result is EvaluateResult.TIMEOUT:
                    plugins.on_fail()
                    fail_count += 1
                else:
                    plugins.on_resync()

                if args.exit_on_error and (fail_count != 0):
                    exit()

                if fail_count > 10:
                    flog.error("10 Failures, power cycling")
                    kasa.power_cycle()
                elif fail_count > 3:
                    evaluator.mcu_reboot()
                    evaluator.soc_reboot()
                else:
                    evaluator.soc_reboot()

                test.log()
        except KeyboardInterrupt:
            exit()
        except UnicodeDecodeError:
            continue
        except RebootRequiredError:
            if kasa:
                kasa.power_cycle()
                sleep(20)
            else:
                raise
