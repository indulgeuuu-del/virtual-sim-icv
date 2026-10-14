"""Offline obstacle geometry and strategy -> real drive -> SetDrive regression.

Windows/MSVC, no SimOne installation. Generated sources/reports go under work/.
Uses the exact production merge geometry, corner functions, drive(), pursuit,
PID and curvature code. Map/API inputs and frame scheduling remain fixtures.
The 2027 scoring PDF p11 table 3 requires braking/stopping without collision;
these engineering checks are prerequisites, not a scoring/physics simulation.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from test_competition_manual import read_source


LOOP_TESTS = (
    'empty', 'defaults', 'explicit', 'interpolation', 'transition',
    'transition_parameters', 'first_route_failure', 'interpolation_failure',
    'transition_route_failure', 'fractional_stop', 'reentry',
)
OUTPUT_TESTS = (
    'output_stop_forward', 'output_stop_reverse', 'output_stop_slide',
    'output_stop_both', 'output_move_reverse', 'output_move_slide',
    'output_percent_zero', 'output_resume_slide',
    'output_route_failure_reverse', 'output_route_failure_slide',
)


def between(text, start, end):
    """Fail if a production boundary disappears; never fall back to copied logic."""
    begin = text.index(start)
    return text[begin:text.index(end, begin)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--label', default='development', help='Separate report directory, e.g. pre-fix')
    parser.add_argument('--revision', help='Test production files at a Git revision with current regression tests')
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_-]+', args.label):
        parser.error('label must contain only letters, digits, underscore or hyphen')
    root = Path(__file__).resolve().parents[1]
    base = root / 'competition'
    output = root / 'work/competition_control' / args.label
    output.mkdir(parents=True, exist_ok=True)
    source_names = (
        'TrajectoryControl/src/core/obstacle.cpp', 'TrajectoryControl/src/core/vehicle.cpp',
        'TrajectoryControl/src/main_manual.cpp', 'TrajectoryControl/src/core/process.cpp',
        'TrajectoryControl/src/core/process.h', 'TrajectoryControl/src/tools/controller.hpp',
        'util/UtilDriver.h',
    )
    report = {'sdk_build_verified': False, 'platform_run_verified': False,
              'official_score_verified': False,
              'scope': 'extracted geometry; full strategy loop and drive body; real pursuit/PID/curvature',
              'limitations': 'map/API/frame-input fixtures; no full update(), SDK, vehicle dynamics or scoring',
              'tests': {}, 'passed': False}
    try:
        revision = None
        if args.revision:
            revision = subprocess.check_output(['git', 'rev-parse', '--verify', '--end-of-options',
                                                args.revision + '^{commit}'], cwd=root, text=True).strip()
        raw_sources = {name: (subprocess.check_output(['git', 'show', f'{revision}:competition/{name}'], cwd=root)
                              if revision else (base / name).read_bytes()) for name in source_names}
        sources = {}
        for name, raw in raw_sources.items():
            try:
                sources[name] = raw.decode('utf-8-sig')
            except UnicodeDecodeError:
                sources[name] = raw.decode('gb18030')
            sources[name] = sources[name].replace('\r\n', '\n')
        inputs = [
            Path(__file__), root / 'tests/cpp/manual_stub/main.h',
            root / 'tests/cpp/manual_stub/test_main.cpp', root / 'tests/cpp/drive_output_test.cpp',
            root / 'tests/cpp/obstacle_geometry_test.cpp', root / 'tools/test_competition_manual.py',
        ]
        report['source_sha256'] = {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                                   for p in inputs}
        report['source_sha256'].update({'competition/' + name: hashlib.sha256(raw).hexdigest()
                                       for name, raw in raw_sources.items()})
        report['production_revision'] = revision or 'working-tree'
        report['git_head'] = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
        obstacle = sources['TrajectoryControl/src/core/obstacle.cpp']
        corners = between(obstacle, 'SSD::SimPoint3D calculateObstacleTL(', 'void groupObstacleByDist(')
        merge = between(obstacle, '        std::vector<SSD::SimPoint3D> tlList, brList;',
                        '        laneID = m_SampleGetNearMostLane(pt);')
        geometry = corners + '\nstruct Merge : Box {\nBox run(const std::vector<size_t>& index) {\n'
        geometry += merge + '\nreturn *this;\n}\n};\n'
        geometry += 'Box mergeGeometry(const std::vector<size_t>& index) { return Merge{}.run(index); }\n'
        harness = read_source(root / 'tests/cpp/obstacle_geometry_test.cpp')
        if harness.count('// @PRODUCTION_GEOMETRY@') != 1:
            raise ValueError('missing geometry insertion point')
        (output / 'geometry.cpp').write_bytes(harness.replace('// @PRODUCTION_GEOMETRY@', geometry).encode('utf-8'))

        drive_dir = output / 'drive'
        drive_dir.mkdir(exist_ok=True)
        for name in ('main.h', 'test_main.cpp'):
            shutil.copyfile(root / 'tests/cpp/manual_stub' / name, drive_dir / name)
        shutil.copyfile(root / 'tests/cpp/drive_output_test.cpp', drive_dir / 'drive_output_test.cpp')
        (drive_dir / 'main_manual.cpp').write_bytes(sources['TrajectoryControl/src/main_manual.cpp'].encode('utf-8'))
        drive = between(sources['TrajectoryControl/src/core/vehicle.cpp'],
                        'void MainVehicle::drive(void)', '// 清除所有邻域信息')
        pursuit = sources['util/UtilDriver.h'].split('class UtilDriver', 1)[1]
        curvature_decl = sources['TrajectoryControl/src/core/process.h'].split('double approximateCurvature(', 1)[1].split(';', 1)[0]
        curvature = sources['TrajectoryControl/src/core/process.cpp'].split('static double dist3D(', 1)[1]
        # Helpers are copied from current production sources, not reimplemented.
        support = sources['TrajectoryControl/src/tools/controller.hpp']
        cpp = '#include "main.h"\n' + support + '\nclass UtilDriver' + pursuit
        cpp += '\ninline PositionalPID steerPID(0,0,0);\n'
        cpp += 'double approximateCurvature(' + curvature_decl + ';\nstatic double dist3D(' + curvature
        cpp += '\n' + drive
        (drive_dir / 'drive.cpp').write_bytes(cpp.encode('utf-8'))

        if os.name != 'nt':
            raise RuntimeError('This runner currently supports Windows/MSVC only')
        vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        install = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
                    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        if not install:
            raise RuntimeError('Install Visual Studio Desktop development with C++')
        vcvars = Path(install) / 'VC/Auxiliary/Build/vcvars64.bat'
        result = subprocess.run(f'call "{vcvars}" >nul && set', shell=True, capture_output=True, check=True)
        env = os.environ.copy()
        for line in result.stdout.decode('mbcs').splitlines():
            key, sep, value = line.partition('=')
            if sep and key and not key.startswith('='):
                env[key] = value
        compiler = Path(env['VCToolsInstallDir']) / 'bin/Hostx64/x64/cl.exe'

        def compile_run(directory, files, names, suite, defines=()):
            result = subprocess.run([str(compiler), '/nologo', '/EHsc', '/std:c++17', '/utf-8',
                                     '/W4', '/Od', '/D_USE_MATH_DEFINES', *defines, *files, '/Fe:test.exe'],
                                    cwd=directory, env=env, capture_output=True, timeout=120)
            (directory / 'compile.log').write_bytes(result.stdout + result.stderr)
            if result.returncode:
                raise RuntimeError(f'Compile failed: {directory / "compile.log"}')
            for name in names:
                result = subprocess.run([str(directory / 'test.exe'), name], cwd=directory, env=env,
                                        capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=10)
                report['tests'][f'{suite}/{name}'] = {
                    'passed': result.returncode == 0, 'output': (result.stdout + result.stderr).strip()}

        compile_run(output, ['geometry.cpp'], [str(i) for i in range(13)], 'geometry')
        compile_run(drive_dir, ['main_manual.cpp', 'drive.cpp', 'test_main.cpp'],
                    LOOP_TESTS + OUTPUT_TESTS, 'drive', ['/DTEST_REAL_DRIVE'])
        report['passed'] = bool(report['tests']) and all(t['passed'] for t in report['tests'].values())
    except (OSError, ValueError, IndexError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        report['error'] = f'{type(error).__name__}: {error}'
    (output / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
