import io
import logging
import unittest

from host.log_format import ColorFormatter, configure_logging, want_color

GREEN, YELLOW, RED, RESET = "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[0m"


def record(level, message="text"):
    return logging.LogRecord("emulator.test", level, __file__, 1, message, None, None)


class Tty(io.StringIO):
    def isatty(self):
        return True


class ColorFormatterTest(unittest.TestCase):
    def setUp(self):
        self.formatter = ColorFormatter("%(levelname)s %(name)s: %(message)s")

    def test_warning_is_yellow(self):
        line = self.formatter.format(record(logging.WARNING))
        self.assertEqual(line, YELLOW + "WARNING emulator.test: text" + RESET)

    def test_error_and_critical_are_red(self):
        for level in (logging.ERROR, logging.CRITICAL):
            line = self.formatter.format(record(level))
            self.assertTrue(line.startswith(RED) and line.endswith(RESET), line)

    def test_info_is_green(self):
        line = self.formatter.format(record(logging.INFO))
        self.assertEqual(line, GREEN + "INFO emulator.test: text" + RESET)

    def test_debug_stays_plain(self):
        self.assertNotIn("\x1b", self.formatter.format(record(logging.DEBUG)))


class WantColorTest(unittest.TestCase):
    def test_a_terminal_gets_color(self):
        self.assertTrue(want_color(Tty(), {}))

    def test_a_pipe_does_not(self):
        self.assertFalse(want_color(io.StringIO(), {}))

    def test_no_color_wins_over_a_terminal(self):
        self.assertFalse(want_color(Tty(), {"NO_COLOR": "1"}))

    def test_force_color_enables_it_on_a_pipe(self):
        self.assertTrue(want_color(io.StringIO(), {"FORCE_COLOR": "1"}))

    def test_no_color_wins_over_force_color(self):
        self.assertFalse(want_color(io.StringIO(), {"NO_COLOR": "1", "FORCE_COLOR": "1"}))

    def test_empty_variables_count_as_unset(self):
        self.assertTrue(want_color(Tty(), {"NO_COLOR": ""}))
        self.assertFalse(want_color(io.StringIO(), {"FORCE_COLOR": ""}))


class ConfigureTest(unittest.TestCase):
    def setUp(self):
        self.addCleanup(configure_logging, io.StringIO(), {})  # leave a plain handler behind

    def test_modules_log_through_the_stream_in_colour(self):
        stream = io.StringIO()
        configure_logging(stream, {"FORCE_COLOR": "1"})
        logging.getLogger("emulator.bus").warning("w")
        logging.getLogger("emulator.flash").error("e")
        out = stream.getvalue()
        self.assertIn(YELLOW + "WARNING emulator.bus: w" + RESET, out)
        self.assertIn(RED + "ERROR emulator.flash: e" + RESET, out)

    def test_info_warning_and_error_always_show_but_debug_only_when_asked(self):
        stream = io.StringIO()
        configure_logging(stream, {})
        quiet, asked = logging.getLogger("emulator.quiet"), logging.getLogger("emulator.asked")
        self.addCleanup(asked.setLevel, logging.NOTSET)
        asked.setLevel(logging.DEBUG)
        for logger in (quiet, asked):
            logger.debug("d-%s", logger.name)
            logger.info("i-%s", logger.name)
            logger.warning("w-%s", logger.name)
            logger.error("e-%s", logger.name)
        out = stream.getvalue()
        for name in ("quiet", "asked"):
            for letter in "iwe":
                self.assertIn("%s-emulator.%s" % (letter, name), out)
        self.assertIn("d-emulator.asked", out)
        self.assertNotIn("d-emulator.quiet", out)

    def test_plain_output_has_no_escape_codes(self):
        stream = io.StringIO()
        configure_logging(stream, {})
        logging.getLogger("emulator.bus").warning("w")
        self.assertEqual(stream.getvalue(), "WARNING emulator.bus: w\n")

    def test_configuring_twice_does_not_duplicate_lines(self):
        stream = io.StringIO()
        configure_logging(io.StringIO(), {})
        configure_logging(stream, {})
        logging.getLogger("emulator.bus").warning("once")
        self.assertEqual(stream.getvalue().count("once"), 1)

    def test_debug_stays_hidden_until_a_module_asks_for_it(self):
        stream = io.StringIO()
        configure_logging(stream, {})
        logging.getLogger("emulator.bus").debug("quiet")
        self.assertEqual(stream.getvalue(), "")


if __name__ == "__main__":
    unittest.main()
