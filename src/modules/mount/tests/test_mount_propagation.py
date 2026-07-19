# SPDX-FileCopyrightText: 2026 Aromatic
# SPDX-License-Identifier: GPL-3.0-or-later

from pathlib import Path
from unittest import mock
import importlib.util
import sys
import tempfile
import types
import unittest

MODULE_PATH = Path(__file__).resolve().parents[1] / "main.py"

fake_utils = types.SimpleNamespace(
    gettext_path=lambda: "",
    gettext_languages=lambda: ["en"],
    warning=lambda _message: None,
    error=lambda _message: None,
    mount=mock.Mock(return_value=0),
)
fake_libcalamares = types.SimpleNamespace(
    utils=fake_utils,
    globalstorage=types.SimpleNamespace(insert=lambda _key, _value: None),
    job=types.SimpleNamespace(configuration={}),
)
sys.modules.setdefault("libcalamares", fake_libcalamares)

spec = importlib.util.spec_from_file_location("mount_main", MODULE_PATH)
mount_module = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(mount_module)


class MountPropagationTests(unittest.TestCase):
    def setUp(self):
        fake_utils.mount.reset_mock(return_value=True)
        fake_utils.mount.return_value = 0

    def test_bind_mount_is_made_recursively_slave(self):
        with tempfile.TemporaryDirectory() as root:
            partition = {
                "device": "/dev",
                "fs": "",
                "mountPoint": "/dev",
                "options": ["bind"],
            }
            mount_options = []

            with mock.patch.object(mount_module.subprocess, "call", return_value=0), mock.patch.object(
                mount_module.subprocess, "check_call"
            ) as check_call:
                mount_module.mount_partition(root, partition, [partition], None, mount_options, None)

            check_call.assert_called_once_with(["mount", "--make-rslave", root + "/dev"])

    def test_regular_mount_does_not_change_propagation(self):
        with tempfile.TemporaryDirectory() as root:
            partition = {
                "device": "/dev/example",
                "fs": "ext4",
                "mountPoint": "/",
            }

            with mock.patch.object(mount_module.subprocess, "call", return_value=0), mock.patch.object(
                mount_module.subprocess, "check_call"
            ) as check_call:
                mount_module.mount_partition(root, partition, [partition], None, [], None)

            check_call.assert_not_called()


if __name__ == "__main__":
    unittest.main()
