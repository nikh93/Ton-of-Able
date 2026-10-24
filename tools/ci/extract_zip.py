"""Extracts a ZIP like 7-Zip would on Windows: backslashes in entry names are
path separators (archives made on Windows sometimes store them that way, and
Linux tools would turn them into file names)."""
import os
import sys
import zipfile

archive, target = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(archive) as z:
    for info in z.infolist():
        name = info.filename.replace('\\', '/')
        path = os.path.normpath(os.path.join(target, name))
        if not path.startswith(os.path.normpath(target) + os.sep):
            continue  # never outside the target
        if name.endswith('/'):
            os.makedirs(path, exist_ok=True)
            continue
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'wb') as out:
            out.write(z.read(info))
