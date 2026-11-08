#!/usr/bin/env python3
"""Program, verify, and boot an Xedge RT1020 image using a full system reset."""
import argparse
import hashlib
import io
from pathlib import Path
import struct
import time

from pyocd.core.helpers import ConnectHelper
from pyocd.core.target import Target
from pyocd.flash.file_programmer import FileProgrammer


FLASH_BASE = 0x60000000
FIRMWARE_SIZE = 4 * 1024 * 1024
VECTOR_OFFSET = 0x2000


def read_image(path):
    data = path.read_bytes()
    if not VECTOR_OFFSET + 8 <= len(data) <= FIRMWARE_SIZE:
        raise ValueError("Image must fit the 4 MiB firmware partition and include its boot header")
    if data[:4] != b"FCFB" or data[0x1000:0x1004] != b"\xd1\x00\x20\x41":
        raise ValueError("Expected a complete RT1020 zephyr.bin with FlexSPI and IVT headers")
    reset_vector = struct.unpack_from("<I", data, VECTOR_OFFSET + 4)[0]
    entry = reset_vector & ~1
    if not reset_vector & 1 or not FLASH_BASE + VECTOR_OFFSET <= entry < FLASH_BASE + len(data):
        raise ValueError("Image reset vector does not point into its QSPI executable code")
    return data, entry


def flash(args, data, entry):
    session = ConnectHelper.session_with_chosen_probe(
        blocking=False, unique_id=args.probe, target_override="mimxrt1020",
        frequency=args.frequency,
        connect_mode="attach" if args.verify_only else "under-reset",
        resume_on_disconnect=False)
    if session is None or session.probe.unique_id != args.probe:
        raise RuntimeError("Exact probe ID not found; select an ID from python -m pyocd list")
    with session:
        target = session.target
        if not args.verify_only:
            # Match the ThreadX RT1020 workflow. Core-only VECTRESET leaves
            # RTOS interrupts active while the RAM flash algorithm executes.
            target.reset_and_halt(Target.ResetType.SYSRESETREQ)
            target.write_core_register("primask", 1)
            target.write32(0xE000E010, 0)  # Disable SysTick.
            target.flush()
            # Sector erase keeps the separate /xedge partition intact.
            FileProgrammer(session, chip_erase="sector", smart_flash=True,
                           trust_crc=False, keep_unwritten=False).program(
                io.BytesIO(data), file_format="bin", base_address=FLASH_BASE)

        # Let ROM initialize FlexSPI and SDRAM before reading mapped flash.
        # Halt at the image entry so application tasks cannot interfere.
        target.reset_and_halt(Target.ResetType.SYSRESETREQ)
        if not target.set_breakpoint(entry, Target.BreakpointType.HW):
            raise RuntimeError("Could not set the application entry breakpoint")
        try:
            target.resume()
            deadline = time.monotonic() + 10
            while target.get_state() != Target.State.HALTED and time.monotonic() < deadline:
                time.sleep(0.01)
            if target.get_state() != Target.State.HALTED:
                target.halt()
                raise RuntimeError("ROM did not reach the application; check SW8 normal QSPI boot settings")
            if target.read_core_register("pc") != entry:
                raise RuntimeError("Processor halted before reaching the application entry")
        finally:
            target.remove_breakpoint(entry)

        for offset in range(0, len(data), 4096):
            expected = data[offset:offset + 4096]
            actual = bytes(target.read_memory_block8(FLASH_BASE + offset, len(expected)))
            if actual != expected:
                raise RuntimeError(f"Firmware readback mismatch at 0x{FLASH_BASE + offset:08x}")
        target.resume()
    print(f"RT1020_FLASH_VERIFIED {hashlib.sha256(data).hexdigest()}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", required=True, help="Exact probe ID from python -m pyocd list")
    parser.add_argument("--image", required=True, type=Path, help="Complete build-rt1020/zephyr/zephyr.bin")
    parser.add_argument("--frequency", type=int, default=1000000, help="SWD clock in Hz (default: 1000000)")
    parser.add_argument("--verify-only", action="store_true",
                        help="Reset, read back, and boot the installed image without programming")
    args = parser.parse_args()
    if args.frequency <= 0:
        parser.error("--frequency must be a positive integer in Hz")
    try:
        data, entry = read_image(args.image)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    flash(args, data, entry)


if __name__ == "__main__":
    main()
