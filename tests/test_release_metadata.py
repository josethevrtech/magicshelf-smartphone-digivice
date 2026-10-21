"""Guard the rename's subscription identity and public release metadata."""
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ReleaseMetadata(unittest.TestCase):
    def test_identity_and_release_links(self):
        creation = json.loads((ROOT / 'creation.json').read_text())
        module = json.loads((ROOT / 'module.json').read_text())
        for meta in (creation, module):
            self.assertEqual(meta['id'], 'org.magicshelf.digital-partner')
            self.assertEqual(meta['name'], 'Smartphone Digivice')
        self.assertEqual(creation['version'], module['version'])
        repo = 'https://github.com/josethevrtech/magicshelf-smartphone-digivice'
        self.assertEqual(creation['repository'], repo)
        self.assertEqual(module['links'][0]['url'], repo)
        for picture in module['screenshots']:
            prefix = repo.replace('github.com', 'raw.githubusercontent.com')
            prefix += '/v' + module['version'] + '/'
            self.assertTrue(picture['url'].startswith(prefix))
            self.assertTrue((ROOT / picture['url'].removeprefix(prefix)).is_file())
        # These are referenced by already-published catalogs and must survive.
        for name in ('digivice.png', 'digivice-back.png'):
            self.assertTrue((ROOT / 'media' / name).is_file())


if __name__ == '__main__':
    unittest.main()
