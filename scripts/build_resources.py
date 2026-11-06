#!/usr/bin/env python3
"""Package Xedge resources privately for one Zephyr build and TPM backend."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import zipfile


def build(source, output, psa):
    source, output = Path(source).resolve(), Path(output).resolve()
    resources = source / 'BAS-Resources'
    stage = output.parent / 'stage'
    work = stage / 'BAS-Resources/build'
    tools = stage / 'BAS-Resources/tools/linux'
    work.mkdir(parents=True, exist_ok=True)
    tools.mkdir(parents=True, exist_ok=True)
    src = stage / 'BAS-Resources/src'
    if not src.exists():
        src.symlink_to(resources / 'src', target_is_directory=True)
    shutil.copy2(resources / 'build/Xedge.sh', work / 'Xedge.sh')
    # Build the host utility with the host compiler, never the ARM toolchain.
    bin2c = tools / 'bin2c'
    subprocess.run(['cc', str(source / 'BAS/tools/bin2c.c'), '-o', str(bin2c)], check=True)
    env = dict(os.environ, SHARKSSL_PSA_CONSUMER=str(psa),
               BA_ENABLE_LUA_OPCUA='0', NO_BIN2C='1')
    subprocess.run(['bash', 'Xedge.sh'], cwd=work, env=env,
                   input='n\n', text=True, check=True)
    archive = output.with_suffix('.zip')
    shutil.copy2(work / 'Xedge.zip', archive)
    tpm = resources / ('src/mako/.lua/psatpm.lua' if psa else 'src/xedge/.lua/tpm.lua')
    with zipfile.ZipFile(archive) as z:
        if z.testzip() is not None or z.read('.lua/tpm.lua') != tpm.read_bytes():
            raise RuntimeError('Xedge resource archive failed TPM/integrity verification')
    subprocess.run([str(bin2c), '-z', 'getLspZipReader', str(archive), str(output)], check=True)
    guard = (f'#if defined(CONFIG_XEDGE_PSA) != {psa}\n'
             '#error Xedge resources do not match the selected TPM backend\n'
             '#endif\n')
    output.write_bytes(guard.encode() + output.read_bytes())


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--psa', type=int, choices=(0, 1), required=True)
    args = parser.parse_args()
    build(args.source, args.output, args.psa)
