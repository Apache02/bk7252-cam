#!/usr/bin/env python3
"""Reference AES generator for the test_security firmware.

Produces the same output as the AES section of main.cpp but computes
ciphertexts/plaintexts with pycryptodome, so the firmware UART log can be
diffed against this script to validate that the hardware engine matches a
trusted reference.
"""

try:
    from Crypto.Cipher import AES
except ImportError:
    raise SystemExit("pycryptodome required: pip install pycryptodome")


# FIPS 197 App. C + NIST SP 800-38A §F.1 ECB examples

PT_FIPS = bytes.fromhex("00112233445566778899aabbccddeeff")
PT_NIST4 = bytes.fromhex(
    "6bc1bee22e409f96e93d7e117393172a"
    "ae2d8a571e03ac9c9eb76fac45af8e51"
    "30c43d2be049dcf10c8a8e6dc1e2b1fe"
    "f69f2445df4f9b17ad2b417be66c4110"
)

AES_VECTORS = {
    # key_bits -> list of (case_name, key_bytes, plaintext_bytes)
    128: [
        ("FIPS-197 single block",
         bytes.fromhex("000102030405060708090a0b0c0d0e0f"),
         PT_FIPS),
        ("NIST SP 800-38A 4 blocks",
         bytes.fromhex("2b7e151628aed2a6abf7158809cf4f3c"),
         PT_NIST4),
    ],
    192: [
        ("FIPS-197 single block",
         bytes.fromhex("000102030405060708090a0b0c0d0e0f1011121314151617"),
         PT_FIPS),
        ("NIST SP 800-38A 4 blocks",
         bytes.fromhex("8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b"),
         PT_NIST4),
    ],
    256: [
        ("FIPS-197 single block",
         bytes.fromhex(
             "000102030405060708090a0b0c0d0e0f"
             "101112131415161718191a1b1c1d1e1f"),
         PT_FIPS),
        ("NIST SP 800-38A 4 blocks",
         bytes.fromhex(
             "603deb1015ca71be2b73aef0857d7781"
             "1f352c073b6108d72d9810a30914dff4"),
         PT_NIST4),
    ],
}


def main() -> None:
    total = 0

    # Encrypt passes
    for bits in (128, 192, 256):
        alg = f"AES-{bits} encrypt"
        print(f"== {alg} ==")
        for case_name, key, pt in AES_VECTORS[bits]:
            ct = AES.new(key, AES.MODE_ECB).encrypt(pt)
            print(f"  [ OK ] {alg} [{case_name}] len={len(pt)} = {ct.hex()}")
            total += 1

    # Decrypt passes — same vectors, swapped
    for bits in (128, 192, 256):
        alg = f"AES-{bits} decrypt"
        print(f"== {alg} ==")
        for case_name, key, pt in AES_VECTORS[bits]:
            ct = AES.new(key, AES.MODE_ECB).encrypt(pt)
            back = AES.new(key, AES.MODE_ECB).decrypt(ct)
            assert back == pt
            print(f"  [ OK ] {alg} [{case_name}] len={len(pt)} = {pt.hex()}")
            total += 1

    print()
    print(f"{total} / {total} passed")


if __name__ == "__main__":
    main()