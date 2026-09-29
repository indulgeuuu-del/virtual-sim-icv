"""Compile the real strategy loop and .stg loader against deterministic doubles, without SimOne."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def read_source(path):
    """Inherited sources are GBK; converted ones are UTF-8 with BOM."""
    raw = path.read_bytes()
    try:
        return raw.decode('utf-8-sig')
    except UnicodeDecodeError:
        return raw.decode('gb18030')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', action='store_true', help='Run the unchanged baseline; failures expected')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    manifest = json.loads((root / 'competition/baseline.json').read_text(encoding='utf-8-sig'))
    base = root / manifest['source_root'] if args.baseline else root / 'competition'
    source = base / 'TrajectoryControl/src/main_manual.cpp'
    parser_source = base / 'TrajectoryControl/src/manual/manual.cpp'
    output = root / 'work/competition_manual' / ('baseline' if args.baseline else 'development')
    output.mkdir(parents=True, exist_ok=True)
    report = {'sources': {path.relative_to(root).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                          for path in (source, parser_source)},
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

        def compile_suite(directory, sources, binary, includes=()):
            result = subprocess.run([str(compiler), '/nologo', '/EHsc', '/std:c++17', '/utf-8',
                                     '/W4', '/Od', '/Zi', *[f'/I{path}' for path in includes],
                                     *sources, f'/Fe:{binary}'],
                                    cwd=directory, env=env, capture_output=True)
            (directory / 'compile.log').write_bytes(result.stdout + result.stderr)
            if result.returncode:
                raise RuntimeError(f'Compile failed: {directory / "compile.log"}')

        def run_suite(suite, directory, binary, names):
            for name in names:
                result = subprocess.run([str(binary), name], cwd=directory, env=env,
                                        capture_output=True, text=True, encoding='utf-8',
                                        errors='replace', timeout=10)
                report['tests'][f'{suite}/{name}'] = {'passed': result.returncode == 0,
                                                      'output': (result.stdout + result.stderr).strip()}

        # Loop: the complete production main_manual.cpp, only normalizing encoding.
        # Its quoted main.h now resolves to the double beside this generated copy.
        (output / 'main_manual.cpp').write_bytes(read_source(source).encode('utf-8'))
        for name in ('main.h', 'test_main.cpp'):
            shutil.copyfile(root / 'tests/cpp/manual_stub' / name, output / name)
        compile_suite(output, ['main_manual.cpp', 'test_main.cpp'], output / 'manual_test.exe')
        run_suite('loop', output, output / 'manual_test.exe',
                  ['empty', 'defaults', 'explicit', 'interpolation', 'transition',
                   'transition_parameters', 'first_route_failure', 'interpolation_failure',
                   'transition_route_failure', 'fractional_stop', 'reentry'])

        # Strategy files: the complete production manual.cpp with its real json.hpp;
        # only SDK types, logger and the SimOne install directory are replaced.
        parser_dir = output / 'strategy'
        if parser_dir.exists():
            shutil.rmtree(parser_dir)
        (parser_dir / 'TrajectoryControl/m_strategy').mkdir(parents=True)
        for name in ('manual.cpp', 'manual.h'):
            # Bytes, so CRLF sources are not newline-translated a second time on Windows.
            (parser_dir / name).write_bytes(read_source(base / 'TrajectoryControl/src/manual' / name)
                                            .encode('utf-8'))
        shutil.copyfile(root / 'competition/TrajectoryControl/m_strategy/41.stg',
                        parser_dir / 'TrajectoryControl/m_strategy/41.stg')
        stub = root / 'tests/cpp/strategy_stub'
        compile_suite(parser_dir, ['manual.cpp', str(stub / 'test_strategy.cpp')],
                      parser_dir / 'strategy_test.exe', [parser_dir, stub, base / 'util/debug'])
        names = subprocess.check_output([str(parser_dir / 'strategy_test.exe'), '--list'],
                                        cwd=parser_dir, env=env, text=True).split()
        run_suite('strategy', parser_dir, parser_dir / 'strategy_test.exe', names)
        report['passed'] = all(item['passed'] for item in report['tests'].values())
    except (OSError, subprocess.SubprocessError, RuntimeError, KeyError) as error:
        report.update(passed=False, error=str(error))
    (output / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
