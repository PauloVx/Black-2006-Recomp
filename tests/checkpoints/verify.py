"""Asset-free process restart, damaged-file, identity and restart regressions."""
import pathlib
import struct
import subprocess
import sys
import tempfile


def hash_bytes(data):
    result = 2166136261
    for byte in data:
        result = ((result ^ byte) * 16777619) & 0xFFFFFFFF
    return result


def main():
    executable = str(pathlib.Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="black-checkpoint-") as directory:
        root = pathlib.Path(directory)

        def run(mode):
            subprocess.run([executable, mode], cwd=root, check=True)

        run("save")
        run("levels")
        path, = (root / "save/checkpoints").glob("*.bin")
        original = path.read_bytes()
        run("load")  # a separate process, different guest allocation addresses
        run("mismatch")
        run("failed-write")
        assert path.read_bytes() == original, "Failed replacement must retain the previous checkpoint"
        for mode in ("binding-before", "binding-after"):
            run(mode)
            run("load-latest")  # newest capture follows the successful profile date
        path.write_bytes(original)
        for damaged in (b"", original[:16], original[:-1], original + b"extra",
                        original[:400] + bytes([original[400] ^ 1]) + original[401:]):
            path.write_bytes(damaged)
            run("reject")
        # Recomputed checksum must not hide incompatible identity/schema/counts.
        for offset, value in ((0, 0), (4, 99), (12, 123), (16, 7), (20, 2),
                              (24, 3), (28, 127), (32, 4097), (36, 128), (40, 0),
                              (120 + 72, 1), (120 + 16, 0x7F800000)):
            damaged = bytearray(original)
            struct.pack_into("<I", damaged, offset, value)
            struct.pack_into("<I", damaged, 8, 0)
            struct.pack_into("<I", damaged, 8, hash_bytes(damaged))
            path.write_bytes(damaged)
            run("reject")
        path.write_bytes(original)
        run("rebind-write")
        run("load-rebound")  # a new native profile date after a successful autosave
        run("clear")
        path.write_bytes(original)
        run("rebind")
        path.write_bytes(original)
        run("retire")


if __name__ == "__main__":
    main()
