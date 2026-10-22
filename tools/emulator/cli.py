"""Command line: tools/emulator/run PROFILE --load TARGET:FILE ... [--uart1 SPEC] [--uart2 SPEC]"""

import argparse
import logging
import os
import signal
import sys
from pathlib import Path

from core.loader import LoadError, apply_loads, parse_load
from core.machine import Machine
from core.profile import ProfileError, load_profile
from host.endpoint import make_endpoint as default_make_endpoint
from host.log_format import configure_logging
from host.net.forward import ForwardError, parse_publish
from host.network import Network
from modules import REGISTRY

PROFILES = Path(__file__).resolve().parent / "profiles"


def _parser():
    parser = argparse.ArgumentParser(prog="run", description="Emulate a BK7252-class SoC.")
    parser.add_argument("profile", help="profile name, upper case (profiles/NAME.toml)")
    parser.add_argument(
        "--load", action="append", default=[], metavar="TARGET:FILE",
        help="load FILE at an alias or address; repeatable",
    )
    parser.add_argument(
        "-p", "--publish", action="append", default=[], metavar="[HOST_IP:]HOST_PORT:GUEST_PORT[/tcp|/udp]",
        help="forward a host port to a port of the firmware, as in Docker; repeatable "
        "(default host address 127.0.0.1, default protocol tcp)",
    )
    parser.add_argument(
        "-d", "--debug", action="append", default=[], metavar="MODULE[,MODULE...]",
        help="print the debug log of modules, decoded (for example gdma,wifi); repeatable",
    )
    for number in (1, 2):
        parser.add_argument(
            "--uart%d" % number, default="pty", metavar="SPEC",
            help="host end of UART%d: pty, stdio or a serial device path (default pty)" % number,
        )
    return parser


def _fail(message):
    print("error: %s" % message, file=sys.stderr)
    return 2


def default_make_network(publishes):
    return Network(publishes)


def main(
    argv=None, make_endpoint=default_make_endpoint, profile_dir=PROFILES, registry=REGISTRY,
    make_network=default_make_network,
):
    args = _parser().parse_args(argv)
    configure_logging(sys.stderr, os.environ)
    for module in (name for arg in args.debug for name in arg.split(",") if name):
        logging.getLogger("emulator." + module).setLevel(logging.DEBUG)

    network = None
    try:
        profile = load_profile(args.profile, profile_dir)
        if not args.load:
            return _fail("at least one --load is required")
        specs = [parse_load(arg) for arg in args.load]
        publishes = [parse_publish(arg) for arg in args.publish]
        machine = Machine(profile, registry, {})
        apply_loads(specs, profile, machine.bus, machine.cpu, machine.modules)
        if publishes:
            if "MAC" not in machine.modules:
                return _fail("--publish needs a profile with a WiFi MAC module named MAC")
            network = make_network(publishes)
            machine.modules["MAC"].attach(network)
            for publish, (host_ip, host_port) in zip(publishes, network.addresses):
                print("%s %s:%d -> guest port %d" % (publish.protocol, host_ip, host_port, publish.guest_port), flush=True)
    except (ProfileError, LoadError, ValueError, ForwardError) as err:
        return _fail(err)

    try:
        return _run(machine, args, make_endpoint, network)
    finally:
        if network is not None:
            network.close()


def _run(machine, args, make_endpoint, network):
    endpoints = []
    stop = []
    previous = {}
    try:
        for number, spec in ((1, args.uart1), (2, args.uart2)):
            module = machine.modules.get("UART%d" % number)
            if module is None:
                continue
            endpoint = make_endpoint(spec)
            endpoints.append(endpoint)
            module.attach(endpoint)
            print("UART%d: %s" % (number, endpoint.description()), flush=True)

        for sig in (signal.SIGINT, signal.SIGTERM):
            previous[sig] = signal.signal(sig, lambda *_: stop.append(True))
        machine.cpu.reset(0)
        machine.scheduler.run_forever(lambda: bool(stop))
    finally:
        for sig, handler in previous.items():
            signal.signal(sig, handler)
        for endpoint in endpoints:
            endpoint.close()
    return 0
