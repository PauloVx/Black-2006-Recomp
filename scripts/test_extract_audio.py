"""Synthetic banks only; no retail audio is included in the test suite."""
import io
from pathlib import Path
import struct
import tempfile
import unittest
import wave

import extract_audio as audio


class BankTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.header = self.root / 'sample.ssh'
        self.body = self.root / 'SAMPLE.BKS'
        self.block = (b'\0\0\x0c\0' + b'\x12' * 15) * 862 + b'\0' * 6
        header = bytearray(40)
        struct.pack_into('<IIBBHI', header, 12, 5, 16384, 1, 2, 1, 32)
        struct.pack_into('<HHI', header, 32, 0, 16, 32000)
        self.header.write_bytes(header)
        self.body.write_bytes(self.block * 2)

    def tearDown(self):
        self.temp.cleanup()

    def test_metadata_and_case_insensitive_pair(self):
        body, block_size, entries = audio.read_bank(self.header)
        self.assertEqual(body, self.body)
        self.assertEqual(block_size, 16384)
        self.assertEqual(entries[0]['samples'], 27584)
        self.assertEqual(entries[0]['sample_rate'], 32000)
        self.assertEqual(entries[0]['channels'], 2)

    def test_channel_boundaries_strip_padding(self):
        _, block_size, entries = audio.read_bank(self.header)
        out = [io.BytesIO(), io.BytesIO()]
        right = self.block.replace(b'\x12', b'\x34')
        entry = dict(entries[0], bytes=65536)
        audio.split_channels(io.BytesIO(self.block + right + self.block + right), entry, block_size, out)
        self.assertEqual(out[0].getvalue(), self.block[:-6] * 2)
        self.assertEqual(out[1].getvalue(), right[:-6] * 2)

    def test_truncation_rejected(self):
        self.body.write_bytes(self.block)
        with self.assertRaisesRegex(ValueError, 'range'):
            audio.read_bank(self.header)

    def test_unsupported_format_rejected(self):
        header = bytearray(self.header.read_bytes())
        struct.pack_into('<I', header, 12, 3)
        self.header.write_bytes(header)
        with self.assertRaisesRegex(ValueError, 'Unsupported SSH'):
            audio.read_bank(self.header)

    def test_invalid_frame_and_padding_rejected(self):
        _, size, entries = audio.read_bank(self.header)
        for offset in (0, 2, 16383):
            with self.subTest(offset=offset):
                bad = bytearray(self.block * 2)
                bad[offset] = 15
                with self.assertRaises(ValueError):
                    audio.split_channels(io.BytesIO(bad), entries[0], size, [io.BytesIO(), io.BytesIO()])

    def test_overlap_rejected(self):
        header = bytearray(self.header.read_bytes())
        struct.pack_into('<H', header, 22, 2)
        header += struct.pack('<HHI', 0, 16, 32000)
        self.header.write_bytes(header)
        with self.assertRaisesRegex(ValueError, 'overlapping'):
            audio.read_bank(self.header)

    def test_existing_output_is_preserved(self):
        _, size, entries = audio.read_bank(self.header)
        target = self.root / 'existing.wav'
        target.write_bytes(b'keep me')
        with self.assertRaises(FileExistsError):
            audio.export_entry(self.body, size, entries[0], target, Path('missing-decoder'))
        self.assertEqual(target.read_bytes(), b'keep me')

    def test_failed_decode_leaves_no_empty_export(self):
        _, size, entries = audio.read_bank(self.header)
        target = self.root / 'failed.wav'
        with self.assertRaises(FileNotFoundError):
            audio.export_entry(self.body, size, entries[0], target, self.root / 'missing-decoder')
        self.assertFalse(target.exists())

    def test_gin_layout(self):
        header = audio.gin_header(64, 32000)
        self.assertEqual(len(header), 40)
        self.assertEqual(header[:4], b'Gnsu')
        self.assertEqual(struct.unpack_from('<II', header, 24), (64, 32000))

    def mono_wav(self, name, samples):
        path = self.root / name
        with wave.open(str(path), 'wb') as writer:
            writer.setparams((1, 2, 32000, 0, 'NONE', 'not compressed'))
            writer.writeframes(struct.pack('<' + 'h' * len(samples), *samples))
        return path

    def test_join_pcm_preserves_channel_order(self):
        left = self.mono_wav('left.wav', [1, 2, -3])
        right = self.mono_wav('right.wav', [-100, 200, 300])
        output = self.root / 'joined.wav'
        audio.join_pcm([left, right], output, dict(sample_rate=32000, samples=3))
        with wave.open(str(output), 'rb') as reader:
            self.assertEqual(reader.getnchannels(), 2)
            self.assertEqual(struct.unpack('<6h', reader.readframes(3)), (1, -100, 2, 200, -3, 300))

    def test_truncated_decoded_pcm_rejected(self):
        for count in (1, 2):
            for removed in (1, 2):
                with self.subTest(channels=count, removed=removed):
                    paths = [self.mono_wav(f'ch-{i}.wav', [1, 2]) for i in range(count)]
                    for path in paths:
                        path.write_bytes(path.read_bytes()[:-removed])
                    with self.assertRaisesRegex(ValueError, 'Partial|Truncated'):
                        audio.join_pcm(paths, self.root / 'bad.wav', dict(sample_rate=32000, samples=2))


if __name__ == '__main__':
    unittest.main()
