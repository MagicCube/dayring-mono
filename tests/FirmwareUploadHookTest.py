"""Exercise the upload handshake without opening a real serial port."""
import contextlib
import io
import itertools
from pathlib import Path
import runpy
import unittest
from unittest.mock import MagicMock, Mock, patch

SCRIPT = Path(__file__).resolve().parents[1] / 'tools/platformio-firmware-update/platformio-firmware-update.py'


class FirmwareUploadHookTest(unittest.TestCase):
    def setUp(self):
        self.env = Mock()
        self.env.subst.return_value = '/dev/fake'
        scope = runpy.run_path(str(SCRIPT), init_globals={'Import': lambda _: None, 'env': self.env})
        self.callback = scope['before_upload']
        self.device = MagicMock()
        self.device.__enter__.return_value = self.device

    def test_ready(self):
        self.env.AddPreAction.assert_called_once_with('upload', self.callback)
        self.device.readline.side_effect = [b'DAYRING PREPARING\r\n', b'DAYRING READY\r\n']
        with patch('serial.Serial', return_value=self.device), contextlib.redirect_stdout(io.StringIO()) as output:
            self.callback(None, None, self.env)
        self.assertIn('screen ready', output.getvalue())
        self.device.write.assert_called_once_with(b'DAYRING PREPARE_UPDATE\n')
        self.assertFalse(self.device.dtr)
        self.assertFalse(self.device.rts)
        self.device.__exit__.assert_called_once()

    def test_timeout(self):
        self.device.readline.return_value = b''
        with patch('serial.Serial', return_value=self.device), patch.dict(
            self.callback.__globals__, monotonic=Mock(side_effect=itertools.count(0, 0.1))
        ), contextlib.redirect_stdout(io.StringIO()) as output:
            self.callback(None, None, self.env)
        self.assertIn('continuing with normal upload', output.getvalue())
        self.device.__exit__.assert_called_once()

    def test_startup_delay_retries_and_partial_reply(self):
        self.device.readline.side_effect = [b'boot\n'] * 6 + [b'DAYRING PRE', b'PARING\r\n', b'DAYRING READY\n']
        with patch('serial.Serial', return_value=self.device), patch.dict(
            self.callback.__globals__, monotonic=Mock(side_effect=itertools.count(0, 0.1))
        ), contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertIsNone(self.callback(None, None, self.env))
        self.assertGreater(self.device.write.call_count, 1)
        self.assertIn('screen ready', output.getvalue())

    def test_accepted_timeout_blocks_upload(self):
        self.device.readline.side_effect = itertools.chain([b'DAYRING PREPARING\n'], itertools.repeat(b''))
        with patch('serial.Serial', return_value=self.device), patch.dict(
            self.callback.__globals__, monotonic=Mock(side_effect=itertools.count(0, 0.1))
        ), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(self.callback(None, None, self.env), 1)
        self.device.write.assert_called_once()

    def test_accepted_disconnect_blocks_upload(self):
        self.device.readline.side_effect = [b'DAYRING PREPARING\n', OSError('disconnected')]
        with patch('serial.Serial', return_value=self.device), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(self.callback(None, None, self.env), 1)

    def test_busy_port(self):
        with patch('serial.Serial', side_effect=OSError('busy')), contextlib.redirect_stdout(io.StringIO()) as output:
            self.callback(None, None, self.env)
        self.assertIn('Continuing with normal upload', output.getvalue())


if __name__ == '__main__':
    unittest.main()
