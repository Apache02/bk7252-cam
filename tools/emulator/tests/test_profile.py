import tempfile
import unittest
from pathlib import Path

from core.profile import ProfileError, load_profile

PROFILES = Path(__file__).resolve().parent.parent / "profiles"

MINIMAL = """
name = "TEST"
insns_per_us = 100
peripheral_window = [0x00800000, 0x00100000]

[[memory]]
name = "RAM1"
base = 0x00400000
size = 0x40000

[alias]
RAM = 0x00400000
{extra}
"""


def profile_from(text, name="TEST"):
    directory = Path(tempfile.mkdtemp())
    (directory / (name + ".toml")).write_text(text)
    return load_profile(name, directory)


class ProfileTest(unittest.TestCase):
    def test_loads_bk7252(self):
        profile = load_profile("BK7252", PROFILES)
        ram2 = [m for m in profile.memory if m.name == "RAM2"][0]
        self.assertEqual(ram2.base, 0x00900000)
        self.assertEqual(profile.aliases["IRAM"].address, 0x00900000)
        app = profile.aliases["APP"]
        self.assertEqual((app.module, app.offset), ("FLASH", 0x11000))

    def test_name_must_be_upper_case(self):
        with self.assertRaisesRegex(ProfileError, "upper case"):
            load_profile("bk7252", PROFILES)

    def test_alias_lookup_case_insensitive(self):
        profile = load_profile("BK7252", PROFILES)
        self.assertIs(profile.alias("app"), profile.aliases["APP"])
        self.assertIsNone(profile.alias("nothing"))

    def test_duplicate_alias_case_insensitive_rejected(self):
        with self.assertRaisesRegex(ProfileError, "duplicate alias"):
            profile_from(MINIMAL.format(extra="ram = 0x00400000"))

    def test_alias_outside_memory_rejected(self):
        with self.assertRaisesRegex(ProfileError, "outside memory"):
            profile_from(MINIMAL.format(extra="BAD = 0x20000000"))

    def test_missing_profile_lists_available(self):
        with self.assertRaisesRegex(ProfileError, "BK7252"):
            load_profile("NOPE", PROFILES)


if __name__ == "__main__":
    unittest.main()
