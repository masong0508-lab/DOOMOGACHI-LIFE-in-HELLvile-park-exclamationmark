import tempfile
import unittest
import wave
from pathlib import Path
from capture_origin import align_capture


class CaptureOriginTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.log = Path(self.tmp.name) / 'dcrun.log'
        self.src = Path(self.tmp.name) / 'dc.wav'
        self.dst = Path(self.tmp.name) / 'game.wav'
        self.rate = 44100
        self.prefix = 30 * self.rate
        self.game = b'\x01\x00\x02\x00' * 10 + b'\x00' * 80000 + b'\x03\x00\x04\x00' * 15
        with wave.open(str(self.src), 'wb') as w:
            w.setparams((2, 2, self.rate, 0, 'NONE', 'not compressed'))
            w.writeframes(b'\0' * (self.prefix * 4) + self.game)

    def marker(self, frame, tick=0):
        return f'dcrun: script audio start frame {frame} (tick {tick})\n'

    def test_long_preload_and_real_rest(self):
        self.log.write_text('talkdisc: bank ready\n' + self.marker(self.prefix))
        align_capture(self.log, self.src, self.dst)
        with wave.open(str(self.dst), 'rb') as w:
            self.assertEqual(w.getframerate(), self.rate)
            self.assertEqual(w.getnframes(), len(self.game) // 4)
            self.assertEqual(w.readframes(w.getnframes()), self.game)
        with wave.open(str(self.src), 'rb') as w:
            self.assertEqual(w.getnframes(), self.prefix + len(self.game) // 4)

    def test_missing_or_ambiguous_marker_rejected(self):
        for log in ('', self.marker(0) * 2, self.marker(0, 1), self.marker(99999999)):
            with self.subTest(log=log):
                self.log.write_text(log)
                with self.assertRaises(ValueError):
                    align_capture(self.log, self.src, self.dst)


if __name__ == '__main__':
    unittest.main()
