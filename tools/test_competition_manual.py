"""Compile the real strategy loop against deterministic doubles, without SimOne."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', action='store_true', help='Run the unchanged baseline; failures expected')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    manifest = json.loads((root / 'competition/baseline.json').read_text(encoding='utf-8-sig'))
    base = root / manifest['source_root'] if args.baseline else root / 'competition'
    source = base / 'TrajectoryControl/src/main_manual.cpp'
    output = root / 'work/competition_manual' / ('baseline' if args.baseline else 'development')
    output.mkdir(parents=True, exist_ok=True)
    report = {'source': source.relative_to(root).as_posix(),
              'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
              'sdk_build_verified': False, 'platform_run_verified': False, 'tests': {}}
    try:
        if os.name != 'nt':
            raise RuntimeError('This runner currently supports Windows/MSVC only')
        vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        install = subprocess.check_output([str(vswhere), '-latest', '-products', '*',
                    '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
                    '-property', 'installationPath'], text=True).strip()
        if not install:
            raise RuntimeError('Install the Visual Studio Desktop development with C++ workload')
        vcvars = Path(install) / 'VC/Auxiliary/Build/vcvars64.bat'
        env_result = subprocess.run(f'call "{vcvars}" >nul && set', shell=True,
                                    capture_output=True, check=True)
        env = os.environ.copy()
        for line in env_result.stdout.decode('mbcs').splitlines():
            key, sep, value = line.partition('=')
            if sep and key and not key.startswith('='):
                env[key] = value
        compiler = Path(env['VCToolsInstallDir']) / 'bin/Hostx64/x64/cl.exe'
        # Copy the complete production translation unit, only normalizing encoding.
        # Its quoted main.h now resolves to the double beside this generated copy.
        raw = source.read_bytes()
        try:
            content = raw.decode('utf-8-sig')
        except UnicodeDecodeError:
            content = raw.decode('gb18030')
        (output / 'main_manual.cpp').write_text(content, encoding='utf-8')
        for name in ('main.h', 'test_main.cpp'):
            shutil.copyfile(root / 'tests/cpp/manual_stub' / name, output / name)
        binary = output / 'manual_test.exe'
        result = subprocess.run([str(compiler), '/nologo', '/EHsc', '/std:c++17', '/utf-8',
                                 '/W4', '/Od', '/Zi', 'main_manual.cpp', 'test_main.cpp',
                                 f'/Fe:{binary}'], cwd=output, env=env, capture_output=True)
        (output / 'compile.log').write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f'Compile failed: {output / "compile.log"}')
        for name in ('empty', 'defaults', 'explicit', 'interpolation', 'transition',
                     'fractional_stop', 'reentry'):
            result = subprocess.run([str(binary), name], cwd=output, env=env,
                                    capture_output=True, text=True, timeout=10)
            report['tests'][name] = {'passed': result.returncode == 0,
                                     'output': (result.stdout + result.stderr).strip()}
        report['passed'] = all(item['passed'] for item in report['tests'].values())
    except (OSError, subprocess.SubprocessError, RuntimeError, KeyError) as error:
        report.update(passed=False, error=str(error))
    (output / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
