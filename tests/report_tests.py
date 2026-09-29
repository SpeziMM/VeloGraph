"""The report must detect source drift and evidence tampering without external services."""
import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('build_report', ROOT/'tools/build_report.py')
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)


class ReportTests(unittest.TestCase):
    def test_historical_counts_are_generated_from_cases_not_repeats(self):
        rows = report.summary(ROOT/'docs/benchmarks/2026-09-29/heldout.csv')
        hybrid = next(r for r in rows if r['engine'] == 'hybrid')
        self.assertEqual((hybrid['cases'], hybrid['valid'], hybrid['in_tolerance'], hybrid['samples']), (60,31,23,180))

    def test_source_add_edit_remove_and_output_tampering(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for folder in ['src', 'include', 'tests', 'tools', 'docs']:
                shutil.copytree(ROOT/folder, root/folder, ignore=shutil.ignore_patterns('__pycache__'))
            report.refresh(root, 'Test fixture source review')
            report.check(root)
            source = root/'src/main.cpp'
            original = source.read_text()
            source.write_text(original+'\n// changed behavior\n')
            with self.assertRaisesRegex(ValueError, 'stale'): report.check(root)
            source.write_text(original)
            added = root/'src/new_stage.cpp'
            added.write_text('// new stage\n')
            with self.assertRaisesRegex(ValueError, 'new_stage'): report.check(root)
            added.unlink()
            source.unlink()
            with self.assertRaisesRegex(ValueError, 'main.cpp'): report.check(root)
            source.write_text(original)
            (root/report.OUTPUT).write_text('tampered')
            with self.assertRaisesRegex(ValueError, 'evidence'): report.check(root)


if __name__ == '__main__':
    unittest.main()
