"""Console logging: info in green, warnings in yellow, errors in red."""

import logging

_GREEN, _YELLOW, _RED, _RESET = "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[0m"
_FORMAT = "%(levelname)s %(name)s: %(message)s"
_ROOT = "emulator"


class ColorFormatter(logging.Formatter):
    def format(self, record):
        line = super().format(record)
        if record.levelno >= logging.ERROR:
            return _RED + line + _RESET
        if record.levelno >= logging.WARNING:
            return _YELLOW + line + _RESET
        if record.levelno >= logging.INFO:
            return _GREEN + line + _RESET
        return line


def want_color(stream, environ):
    """NO_COLOR turns colour off, FORCE_COLOR turns it on, otherwise a terminal gets it."""
    if environ.get("NO_COLOR"):
        return False
    if environ.get("FORCE_COLOR"):
        return True
    return stream.isatty()


def configure_logging(stream, environ):
    """Send every emulator.* logger to `stream`; replaces an earlier call's handler."""
    logger = logging.getLogger(_ROOT)
    for handler in [h for h in logger.handlers if getattr(h, "emulator_console", False)]:
        logger.removeHandler(handler)
    handler = logging.StreamHandler(stream)
    handler.emulator_console = True
    formatter = ColorFormatter if want_color(stream, environ) else logging.Formatter
    handler.setFormatter(formatter(_FORMAT))
    logger.addHandler(handler)
    logger.setLevel(logging.INFO)  # a module logger set to DEBUG by --debug overrides this
    logger.propagate = False
