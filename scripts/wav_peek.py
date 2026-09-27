import argparse
import struct
import sys

FORMAT_NAMES = {1: "PCM integer", 3: "IEEE float", 0xFFFE: "extensible"}
FLOAT_GUID_PREFIX = struct.pack("<H", 3)


def read_chunks(f):
    riff, _, wave = struct.unpack("<4sI4s", f.read(12))
    if riff != b"RIFF" or wave != b"WAVE":
        sys.exit("not a WAV file")
    fmt = None
    while True:
        header = f.read(8)
        if len(header) < 8:
            sys.exit("no data chunk")
        chunk_id, size = struct.unpack("<4sI", header)
        if chunk_id == b"fmt ":
            fmt = f.read(size)
        elif chunk_id == b"data":
            return fmt, size, f.tell()
        else:
            f.seek(size + (size & 1), 1)


def parse_fmt(fmt):
    tag, channels, rate, byte_rate, block_align, bits = struct.unpack("<HHIIHH", fmt[:16])
    is_float = tag == 3
    if tag == 0xFFFE:
        is_float = fmt[24:26] == FLOAT_GUID_PREFIX
    return tag, channels, rate, byte_rate, block_align, bits, is_float


def bar(value, width=20):
    filled = min(width, round(abs(value) * width))
    left = " " * (width - filled) + "#" * filled if value < 0 else " " * width
    right = "#" * filled + " " * (width - filled) if value >= 0 else " " * width
    return f"{left}|{right}"


def main():
    parser = argparse.ArgumentParser(description="Show a WAV header and a few samples as text")
    parser.add_argument("path")
    parser.add_argument("--at", type=float, default=0.0, help="start time in seconds")
    parser.add_argument("--frames", type=int, default=20, help="how many frames to print")
    args = parser.parse_args()

    with open(args.path, "rb") as f:
        fmt, data_size, data_offset = read_chunks(f)
        tag, channels, rate, byte_rate, block_align, bits, is_float = parse_fmt(fmt)
        total_frames = data_size // block_align

        print(f"format        {FORMAT_NAMES.get(tag, tag)} ({'float' if is_float else 'int'}{bits})")
        print(f"channels      {channels}")
        print(f"sample rate   {rate} Hz")
        print(f"bytes/frame   {block_align}")
        print(f"frames        {total_frames}")
        print(f"duration      {total_frames / rate:.2f} s")
        print(f"data size     {data_size / 1_000_000:.1f} MB")
        print()

        start = min(int(args.at * rate), total_frames)
        count = min(args.frames, total_frames - start)
        f.seek(data_offset + start * block_align)
        raw = f.read(count * block_align)

        sample_bytes = bits // 8
        if is_float and bits == 32:
            code, scale = "f", 1.0
        elif bits == 16:
            code, scale = "h", 32768.0
        elif bits == 32:
            code, scale = "i", 2147483648.0
        else:
            sys.exit(f"unsupported sample size: {bits} bits")

        values = struct.unpack(f"<{count * channels}{code}", raw[: count * channels * sample_bytes])
        names = ["L", "R"] if channels == 2 else [f"ch{c}" for c in range(channels)]

        print(f"frames {start}..{start + count - 1} (t = {start / rate:.4f} s)")
        for i in range(count):
            cells = []
            for c in range(channels):
                v = values[i * channels + c] / scale
                cells.append(f"{names[c]} {v:+.5f} {bar(v)}")
            print(f"{start + i:>9}  " + "   ".join(cells))


if __name__ == "__main__":
    main()
