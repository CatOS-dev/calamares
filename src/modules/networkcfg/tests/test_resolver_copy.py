from pathlib import Path
import importlib.util
import sys
import tempfile
import types
import unittest

MODULE_PATH = Path(__file__).resolve().parents[1] / "main.py"

fake_libcalamares = types.SimpleNamespace(
    utils=types.SimpleNamespace(
        gettext_path=lambda: "",
        gettext_languages=lambda: ["en"],
        debug=lambda _message: None,
        warning=lambda _message: None,
    ),
    globalstorage=types.SimpleNamespace(value=lambda _key: None),
)
sys.modules.setdefault("libcalamares", fake_libcalamares)

spec = importlib.util.spec_from_file_location("networkcfg_main", MODULE_PATH)
networkcfg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(networkcfg)


class ResolverCopyTests(unittest.TestCase):
    def test_copy_resolver_dereferences_source_symlink(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            live_run = root / "live-run"
            live_run.mkdir()
            resolver = live_run / "resolv.conf"
            resolver.write_text("nameserver 192.0.2.1\n", encoding="utf-8")
            source = root / "source-resolv.conf"
            source.symlink_to(resolver)
            target = root / "target-resolv.conf"
            target.write_text("old\n", encoding="utf-8")

            networkcfg.copy_resolver_config(source, target)

            self.assertFalse(target.is_symlink())
            self.assertEqual(target.read_text(encoding="utf-8"), "nameserver 192.0.2.1\n")


if __name__ == "__main__":
    unittest.main()
