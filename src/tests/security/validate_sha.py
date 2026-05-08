#!/usr/bin/env python3
"""Reference SHA hash generator for the test_security firmware.

Produces the same output as main.cpp but computes digests with Python's
hashlib, so the firmware UART log can be diffed against this script to
validate that the hardware engine matches a trusted reference.
"""

import hashlib


MSG_EMPTY = ""
MSG_ABC = "abc"
MSG_FOX = "The quick brown fox jumps over the lazy dog"
MSG_NIST_56 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
MSG_NIST_112 = (
    "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
    "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu"
)


SMALL_CASES = [MSG_EMPTY, MSG_ABC, MSG_FOX, MSG_NIST_56]
LARGE_CASES = [MSG_EMPTY, MSG_ABC, MSG_FOX, MSG_NIST_112]
LARGE_T_CASES = [MSG_EMPTY, MSG_ABC, MSG_NIST_112]


ALGORITHMS = [
    ("SHA-1",       "sha1",       SMALL_CASES),
    ("SHA-224",     "sha224",     SMALL_CASES),
    ("SHA-256",     "sha256",     SMALL_CASES),
    ("SHA-384",     "sha384",     LARGE_CASES),
    ("SHA-512",     "sha512",     LARGE_CASES),
    ("SHA-512/224", "sha512_224", LARGE_T_CASES),
    ("SHA-512/256", "sha512_256", LARGE_T_CASES),
]


def main() -> None:
    total = 0
    for display_name, hashlib_name, messages in ALGORITHMS:
        print(f"== {display_name} ==")
        for msg in messages:
            data = msg.encode("ascii")
            digest = hashlib.new(hashlib_name, data).hexdigest()
            print(f"  [ OK ] {display_name}(len={len(data)}) = {digest}")
            total += 1
    print()
    print(f"{total} / {total} passed")


if __name__ == "__main__":
    main()
