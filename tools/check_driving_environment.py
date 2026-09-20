"""Verify the Windows compiler and inventory a supplied SDK; never start SimOne."""
import argparse
import json
import os
from pathlib import Path
import subprocess
from datetime import datetime, timezone


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk-root', type=Path, help='Local SDK directory, if available')
    args = parser.parse_args()
    if args.sdk_root is not None and not args.sdk_root.is_dir():
        parser.error('--sdk-root must be an existing directory')
    root = Path(__file__).resolve().parents[1]
    output = root / 'work/driving_environment'
    output.mkdir(parents=True, exist_ok=True)
    result = {'checked_at': datetime.now(timezone.utc).isoformat(),
              'compiler_smoke_passed': False, 'sdk_supplied': args.sdk_root is not None,
              'simulator_run_verified': False, 'competition_compatibility_verified': False}
    required = ['SimOneServiceAPI.h', 'SimOnePNCAPI.h', 'SimOneSensorAPI.h',
                'SimOneHDMapAPI.h', 'SimOneIOStruct.h', 'SimOneAPI.lib', 'SSD.lib',
                'HDMapModule.lib', 'ProtobufModule.lib', 'SimOneAPI.dll',
                'SSD.dll', 'HDMapModule.dll']
    if args.sdk_root is not None:
        sdk = args.sdk_root.resolve()
        result['sdk_files'] = {name: [str(p.relative_to(sdk)) for p in sdk.rglob(name)]
                               for name in required}
        result['sdk_missing_files'] = [name for name, paths in result['sdk_files'].items() if not paths]
    try:
        if os.name != 'nt':
            raise RuntimeError('This probe currently supports Windows/MSVC only')
        vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        install = subprocess.check_output([str(vswhere), '-latest', '-products', '*',
                    '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
                    '-property', 'installationPath'], text=True).strip()
        if not install:
            raise RuntimeError('Visual Studio C++ x64 tools not found')
        # Capture only in memory; the developer environment can contain credentials.
        vcvars = Path(install) / 'VC/Auxiliary/Build/vcvars64.bat'
        command = f'call "{vcvars}" >nul && set'
        completed = subprocess.run(command, shell=True, capture_output=True, check=True)
        env = os.environ.copy()
        for line in completed.stdout.decode('mbcs').splitlines():
            key, separator, value = line.partition('=')
            if separator and key and not key.startswith('='):
                env[key] = value
        compiler = Path(env['VCToolsInstallDir']) / 'bin/Hostx64/x64/cl.exe'
        binary = output / 'toolchain_smoke.exe'
        compile_run = subprocess.run([str(compiler), '/nologo', '/EHsc', '/std:c++17',
                          str(root / 'tests/cpp/toolchain_smoke.cpp'), f'/Fe:{binary}'],
                          cwd=output, env=env, capture_output=True)
        (output / 'compile.log').write_bytes(compile_run.stdout + compile_run.stderr)
        if compile_run.returncode:
            raise RuntimeError('Compiler failed; see work/driving_environment/compile.log')
        subprocess.run([str(binary)], cwd=output, env=env, check=True)
        result.update(compiler_smoke_passed=True, toolset=env['VCToolsVersion'].strip(),
                      compiler=str(compiler))
    except (OSError, subprocess.SubprocessError, RuntimeError, KeyError) as error:
        result['error'] = str(error)
    result['limitations'] = 'File presence is not ABI, license, map, runtime or competition validation.'
    (output / 'report.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result, indent=2))
    return 0 if result['compiler_smoke_passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
