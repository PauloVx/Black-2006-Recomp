#!/usr/bin/env python3
"""List/export BLACK Xbox SSH+BKS audio using a separately installed vgmstream.

No game data or decoder is distributed. The supported Xbox banks contain EA
XAS v0 (19 bytes/32 samples), in 0x4000-byte channel blocks with six padding
bytes. Feeding the padded BKS directly to a raw decoder desynchronizes it.
"""
import argparse
import array
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import wave


def read_bank(header):
    header = Path(header)
    data = header.read_bytes()
    if len(data) < 32:
        raise ValueError("Truncated SSH header")
    codec, block_size = struct.unpack_from("<II", data, 12)
    version, channels, count = struct.unpack_from("<BBH", data, 20)
    table = struct.unpack_from("<I", data, 24)[0]
    if (codec, block_size, version) != (5, 0x4000, 1) or channels not in (1, 2):
        raise ValueError("Unsupported SSH format; this tool requires BLACK Xbox XAS banks")
    if table < 32 or not count or table + count * 8 > len(data):
        raise ValueError("Invalid SSH record table")
    matches = [p for p in header.parent.iterdir()
               if p.is_file() and p.name.lower() == header.with_suffix('.bks').name.lower()]
    if len(matches) != 1:
        raise ValueError("Expected one matching BKS file beside the SSH header")
    body = matches[0]
    size = body.stat().st_size
    entries = []
    previous_end = 0
    for index in range(count):
        sector, sectors, rate = struct.unpack_from("<HHI", data, table + index * 8)
        offset, length = sector * 2048, sectors * 2048
        if not length or offset < previous_end or offset + length > size:
            raise ValueError(f"Invalid/overlapping BKS range for entry {index + 1}")
        if length % (block_size * channels) or not 8000 <= rate <= 192000:
            raise ValueError(f"Unsupported block alignment/sample rate for entry {index + 1}")
        samples = length // (block_size * channels) * (block_size // 19) * 32
        entries.append(dict(index=index + 1, offset=offset, bytes=length,
                            sample_rate=rate, channels=channels, samples=samples,
                            seconds=samples / rate))
        previous_end = offset + length
    return body, block_size, entries


def split_channels(source, entry, block_size, output_files):
    """Strip each channel block's padding, never carry it into the next frame."""
    useful = block_size // 19 * 19
    source.seek(entry['offset'])
    for block in range(entry['bytes'] // block_size):
        data = source.read(block_size)
        if len(data) != block_size:
            raise ValueError("Truncated BKS payload")
        if any(data[useful:]):
            raise ValueError("Unexpected nonzero XAS block padding")
        for pos in range(0, useful, 19):
            # Four predictor pairs and shift 0..12 in the supported stock banks.
            predictor, shift = data[pos] & 15, data[pos + 2] & 15
            if predictor > 3 or shift > 12:
                raise ValueError("Invalid XAS frame; wrong format or corrupt bank")
        output_files[block % entry['channels']].write(data[:useful])


def gin_header(samples, rate):
    # vgmstream's documented open-source GIN parser routes to XAS v0. Empty RPM
    # tables give a 40-byte header, allowing the verified decoder to be reused.
    return struct.pack('<4s9I', b'Gnsu', 0, 0, 0, 0, 0, samples, rate, 0, 0)


def join_pcm(channels, target, expected):
    readers = []
    try:
        for path in channels:
            readers.append(wave.open(str(path), 'rb'))
        for reader in readers:
            if (reader.getnchannels(), reader.getsampwidth(), reader.getframerate(),
                    reader.getnframes()) != (1, 2, expected['sample_rate'], expected['samples']):
                raise ValueError("Decoder output does not match SSH metadata")
        with wave.open(str(target), 'wb') as writer:
            writer.setparams((len(readers), 2, expected['sample_rate'], 0, 'NONE', 'not compressed'))
            frames = 0
            while True:
                chunks = [r.readframes(32768) for r in readers]
                if any(len(chunk) % 2 for chunk in chunks):
                    raise ValueError("Partial decoded PCM frame")
                if any(len(chunk) != len(chunks[0]) for chunk in chunks):
                    raise ValueError("Unequal decoded channel lengths")
                if not chunks[0]:
                    break
                frames += len(chunks[0]) // 2
                if len(readers) == 1:
                    writer.writeframesraw(chunks[0])
                else:
                    left, right = (array.array('h', chunk) for chunk in chunks)
                    if len(left) != len(right):
                        raise ValueError("Unequal decoded channel lengths")
                    stereo = array.array('h', [0]) * (len(left) * 2)
                    stereo[0::2], stereo[1::2] = left, right
                    writer.writeframesraw(stereo.tobytes())
            if frames != expected['samples']:
                raise ValueError("Truncated decoded PCM payload")
    finally:
        for reader in readers:
            reader.close()


def export_entry(body, block_size, entry, destination, decoder):
    destination = Path(destination)
    # Exclusive reservation also protects an existing symlink/file. Never
    # truncate a user's earlier export if a later decoder invocation fails.
    with destination.open('xb'):
        pass
    try:
        with tempfile.TemporaryDirectory(prefix='black-audio-') as temp:
            folder = Path(temp)
            paths = [folder / f'channel-{ch}.gin' for ch in range(entry['channels'])]
            handles = []
            try:
                for path in paths:
                    handle = path.open('wb')
                    handle.write(gin_header(entry['samples'], entry['sample_rate']))
                    handles.append(handle)
                with body.open('rb') as source:
                    split_channels(source, entry, block_size, handles)
            finally:
                for handle in handles:
                    handle.close()
            wavs = []
            for path in paths:
                wav = path.with_suffix('.wav')
                result = subprocess.run([str(decoder), '-i', '-o', str(wav), str(path)],
                                        capture_output=True, text=True)
                if result.returncode:
                    raise ValueError(f"vgmstream failed: {result.stderr.strip() or result.stdout.strip()}")
                wavs.append(wav)
            joined = folder / 'joined.wav'
            join_pcm(wavs, joined, entry)
            # Copy into our reserved file; output is outside the original bank.
            with joined.open('rb') as source, destination.open('wb') as out:
                import shutil
                shutil.copyfileobj(source, out)
    except BaseException:
        # This function created the reservation. Single-file removal only;
        # neither source data nor an existing export is ever deleted.
        destination.unlink(missing_ok=True)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('header', type=Path, help='SSH file beside its matching BKS')
    parser.add_argument('--list', action='store_true', help='Print metadata only; no decoder needed')
    parser.add_argument('--entry', type=int, action='append', help='One-based entry to export (repeatable)')
    parser.add_argument('--all', action='store_true', help='Export every entry in this bank')
    parser.add_argument('--output', type=Path, help='Directory for WAV files and JSON manifest')
    parser.add_argument('--vgmstream', type=Path, help='Path to vgmstream-cli.exe')
    args = parser.parse_args()
    try:
        body, block_size, entries = read_bank(args.header)
        if args.list:
            print(json.dumps(entries, indent=2))
            return
        if not args.output or not args.vgmstream or not (args.entry or args.all):
            parser.error('Export requires --output, --vgmstream, and --entry or --all')
        if args.entry and args.all:
            parser.error('Choose --entry or --all')
        indices = list(range(1, len(entries) + 1)) if args.all else sorted(set(args.entry))
        if any(i < 1 or i > len(entries) for i in indices):
            parser.error(f'Entry must be between 1 and {len(entries)}')
        decoder = args.vgmstream.resolve(strict=True)
        output = args.output.resolve()
        if output == body.parent.resolve():
            parser.error('Export to a separate directory from the original banks')
        output.mkdir(parents=True, exist_ok=True)
        manifest_path = output / (args.header.stem + '-manifest.json')
        if manifest_path.exists():
            raise ValueError(f'Manifest already exists: {manifest_path}')
        written = []
        for index in indices:
            entry = entries[index - 1]
            destination = output / f'{args.header.stem}-{index:04d}.wav'
            export_entry(body, block_size, entry, destination, decoder)
            written.append(dict(entry, file=destination.name))
            print(f'{destination.name}: {entry["seconds"]:.3f}s, {entry["sample_rate"]} Hz, {entry["channels"]} channel(s)', flush=True)
        manifest = dict(header=str(args.header.resolve()), bank=str(body.resolve()),
                        codec='EA XAS v0', padding_per_channel_block=6,
                        duration_note='Includes stock end padding; no subtitle timing is inferred.',
                        entries=written)
        with manifest_path.open('x', encoding='utf-8') as out:
            json.dump(manifest, out, indent=2)
            out.write('\n')
    except (ValueError, OSError, EOFError, struct.error, wave.Error) as error:
        parser.exit(1, f'Audio extraction failed: {error}\n')


if __name__ == '__main__':
    main()
