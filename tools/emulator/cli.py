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
        "--debug", action="append", default=[], metavar="MODULE",
        help="print the debug log of a module, decoded (for example gdma); repeatable",
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


def main(argv=None, make_endpoint=default_make_endpoint, profile_dir=PROFILES, registry=REGISTRY):
    args = _parser().parse_args(argv)
    configure_logging(sys.stderr, os.environ)
    for module in args.debug:
        logging.getLogger("emulator." + module).setLevel(logging.DEBUG)

    try:
        profile = load_profile(args.profile, profile_dir)
        if not args.load:
            return _fail("at least one --load is required")
        specs = [parse_load(arg) for arg in args.load]
        machine = Machine(profile, registry, {})
        apply_loads(specs, profile, machine.bus, machine.cpu, machine.modules)
    except (ProfileError, LoadError, ValueError) as err:
        return _fail(err)

    return _run(machine, args, make_endpoint)


def _run(machine, args, make_endpoint):
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
