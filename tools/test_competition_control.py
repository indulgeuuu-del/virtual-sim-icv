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
    'timing_moving', 'timing_paused', 'timing_roll_again', 'timing_leave_zone',
    'timing_bad_gps', 'timing_clock_backwards', 'timing_gap', 'timing_boundary',
    'timing_switch_stop', 'timing_bad_obstacle', 'timing_bad_projection',
    'timing_projection_drive', 'timing_projection_switch',
)
OUTPUT_TESTS = (
    'output_stop_forward', 'output_stop_reverse', 'output_stop_slide',
    'output_stop_both', 'output_move_reverse', 'output_move_slide',
    'output_percent_zero', 'output_resume_slide',
    'output_route_failure_reverse', 'output_route_failure_slide',
    'output_follow_stop', 'output_follow_slow', 'output_follow_stop_resume',
    'output_empty_path', 'output_short_path', 'output_empty_reverse',
    'output_empty_slide', 'output_empty_recovery', 'output_explicit_no_path',
    'output_arbitration_stop', 'output_arbitration_reverse', 'output_arbitration_slide',
    'output_arbitration_release', 'output_arbitration_disabled', 'output_arbitration_front',
    'output_arbitration_left_front', 'output_arbitration_right_front',
    'output_arbitration_bad_projection',
    'output_limit_high', 'output_limit_equal', 'output_limit_below', 'output_limit_reverse',
    'output_limit_slide', 'output_limit_overspeed_slide', 'output_limit_zero',
    'output_limit_nan', 'output_limit_negative', 'output_limit_percent',
    'output_invalid_nan', 'output_invalid_infinite', 'output_invalid_negative',
    'output_invalid_reverse', 'output_invalid_slide', 'output_invalid_percent',
    'output_invalid_steering', 'output_send_failure',
    'output_reverse_slow', 'output_reverse_percent', 'output_reverse_recover',
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
        'TrajectoryControl/src/main.cpp', 'TrajectoryControl/src/main_follow.cpp',
        'TrajectoryControl/src/main_track.cpp',
    )
    report = {'sdk_build_verified': False, 'platform_run_verified': False,
              'official_score_verified': False,
              'scope': 'geometry, GPS-update and track input prefixes; full strategy and following loops; obstacle fetch; real drive/pursuit/PID/curvature',
              'limitations': 'map/API/frame-input fixtures; following/track input suites substitute drive; no full update()/track planner, SDK, vehicle dynamics or scoring',
              'tests': {}, 'passed': False}
    try:
        revision = None
        if args.revision:
            revision = subprocess.check_output(['git', 'rev-parse', '--verify', '--end-of-options',
                                                args.revision + '^{commit}'], cwd=root, text=True).strip()
        raw_sources = {name: (subprocess.check_output(['git', 'show', f'{revision}:competition/{name}'], cwd=root)
                              if revision else (base / name).read_bytes()) for name in source_names}
        if b'StopController.h' in raw_sources['TrajectoryControl/src/main_manual.cpp']:
            name = 'util/StopController.h'
            raw_sources[name] = (subprocess.check_output(['git', 'show', f'{revision}:competition/{name}'], cwd=root)
                                 if revision else (base / name).read_bytes())
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
            root / 'tests/cpp/manual_stub/test_stop_timing.cpp',
            root / 'tests/cpp/gps_update_gate.cpp',
            root / 'tests/cpp/following_drive_support.cpp',
            root / 'tests/cpp/follow_input_stub/main.h', root / 'tests/cpp/follow_input_stub/test.cpp',
            root / 'tests/cpp/navigation_test.cpp',
            root / 'tests/cpp/lane_distance_test.cpp',
            root / 'tests/cpp/lateral_gap_test.cpp',
            root / 'tests/cpp/track_speed_support.cpp',
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
        gps_prefix = between(sources['TrajectoryControl/src/core/vehicle.cpp'],
                             'bool MainVehicle::update(', '// 计算主车的详细参数')
        gps_prefix += '\n++processed; return FlagType::isMainVehicleInitialized;\n}\n'
        gps_harness = read_source(root / 'tests/cpp/gps_update_gate.cpp')
        if gps_harness.count('// @GPS_PREFIX@') != 1:
            raise ValueError('missing GPS prefix insertion point')
        (output / 'gps_gate.cpp').write_bytes(gps_harness.replace('// @GPS_PREFIX@', gps_prefix).encode('utf-8'))
        navigation = between(sources['TrajectoryControl/src/main.cpp'], 'bool initNavigation(', 'bool updateObstacleData(')
        navigation_harness = read_source(root / 'tests/cpp/navigation_test.cpp')
        if navigation_harness.count('// @NAVIGATION@') != 1:
            raise ValueError('missing navigation insertion point')
        (output / 'navigation.cpp').write_bytes(navigation_harness.replace('// @NAVIGATION@', navigation).encode('utf-8'))
        lane_distance = between(sources['TrajectoryControl/src/core/process.cpp'], 'float getDistS(', '// 查看有无斑马线')
        distance_harness = read_source(root / 'tests/cpp/lane_distance_test.cpp')
        if distance_harness.count('// @LONGITUDINAL_DISTANCE@') != 1:
            raise ValueError('missing distance insertion point')
        (output / 'lane_distance.cpp').write_bytes(distance_harness.replace('// @LONGITUDINAL_DISTANCE@', lane_distance).encode('utf-8'))
        (output / 'lane_distance_support.hpp').write_bytes(distance_harness.replace(
            '// @LONGITUDINAL_DISTANCE@', lane_distance).split('int main(',1)[0].encode('utf-8'))
        track = sources['TrajectoryControl/src/main_track.cpp']
        if 'auto predictedLateralGap =' in track:
            if track.count('predictedLateralGap(*vehicle)') != 7:
                raise ValueError('review all lateral-gap call sites')
            gap_helper = between(track, 'auto predictedLateralGap =', '// 如果左前邻域有车')
        else:
            legacy_gap = 'getDistT(targetPath.back(), vehicle->predictionGM.back())'
            if track.count(legacy_gap) != 7:
                raise ValueError('review legacy lateral-gap extraction')
            gap_helper = 'auto predictedLateralGap = [&](const Obstacle& o) { const Obstacle* vehicle=&o; return ' + legacy_gap + '; };\n'
        gap_harness = read_source(root / 'tests/cpp/lateral_gap_test.cpp')
        (output / 'lateral_gap.cpp').write_bytes(gap_harness.replace('// @PREDICTED_GAP@', gap_helper).encode('utf-8'))

        drive_dir = output / 'drive'
        drive_dir.mkdir(exist_ok=True)
        if 'util/StopController.h' in sources:
            (drive_dir / 'StopController.h').write_bytes(sources['util/StopController.h'].encode('utf-8'))
        for name in ('main.h', 'test_main.cpp', 'test_stop_timing.cpp'):
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
        following = read_source(root / 'tests/cpp/following_drive_support.cpp')
        replacements = {
            '// @PRODUCTION_CONTROLLER@': support,
            '// @PRODUCTION_NEAREST@': between(obstacle, 'bool calculateNearestObstacle(', '// 找到所有在主车后方的障碍物'),
            '// @PRODUCTION_FOLLOWING@': between(obstacle, 'float caculateFollowingSpeed(', '// 计算超车速度'),
        }
        for marker, code in replacements.items():
            if following.count(marker) != 1:
                raise ValueError('missing following insertion point')
            following = following.replace(marker, code)
        (drive_dir / 'following.cpp').write_bytes(following.encode('utf-8'))
        cpp = '#include "main.h"\n' + support + '\nclass UtilDriver' + pursuit
        cpp += '\ninline PositionalPID steerPID(0,0,0);\n'
        cpp += 'double approximateCurvature(' + curvature_decl + ';\nstatic double dist3D(' + curvature
        cpp += '\n' + drive
        (drive_dir / 'drive.cpp').write_bytes(cpp.encode('utf-8'))
        track = sources['TrajectoryControl/src/main_track.cpp']
        speed_support = read_source(root / 'tests/cpp/track_speed_support.cpp')
        speed_parts = {
            '// @BEFORE_OVERTAKING@': between(track, '\t\t// 超车速度规划', '\t\tif (overtakeSwitcher.state'),
            '// @INJECT_PROJECTION_FAILURE@': ('lateralProjectionFailed = projectionFailed;' if 'bool lateralProjectionFailed' in track else '(void)projectionFailed;'),
            '// @ARBITRATION_AND_FINAL_GUARD@': between(track, '\t\t\t// 1. LF、LB', '\t\tmainVehicle.drive();'),
        }
        for marker, code in speed_parts.items():
            if speed_support.count(marker) != 1:
                raise ValueError('missing speed arbitration insertion point')
            speed_support = speed_support.replace(marker, code)
        (drive_dir / 'track_speed.cpp').write_bytes(speed_support.encode('utf-8'))

        follow_dir = output / 'follow_input'
        follow_dir.mkdir(exist_ok=True)
        for name in ('main.h', 'test.cpp'):
            shutil.copyfile(root / 'tests/cpp/follow_input_stub' / name, follow_dir / name)
        (follow_dir / 'controller.hpp').write_bytes(support.encode('utf-8'))
        (follow_dir / 'main_follow.cpp').write_bytes(sources['TrajectoryControl/src/main_follow.cpp'].encode('utf-8'))
        fetch = sources['TrajectoryControl/src/main.cpp'].split('bool updateObstacleData(', 1)[1]
        gps_gate = between(sources['TrajectoryControl/src/core/vehicle.cpp'],
                           'bool MainVehicle::update(', '// 计算主车的详细参数')
        follow_support = '#include "main.h"\n' + gps_gate + '\nreturn true;\n}\n'
        follow_support += 'bool updateObstacleData(' + fetch + '\n'
        follow_support += replacements['// @PRODUCTION_NEAREST@'] + replacements['// @PRODUCTION_FOLLOWING@']
        (follow_dir / 'support.cpp').write_bytes(follow_support.encode('utf-8'))
        # Execute the real track loop through its fresh-input gate. The much
        # larger downstream planner is deliberately represented by drive().
        track_prefix = sources['TrajectoryControl/src/main_track.cpp'].split(
            '\t\tFlagType::isCrosswalkExist = getCrossWalk();', 1)
        if len(track_prefix) != 2:
            raise ValueError('missing track input boundary')
        (follow_dir / 'track_prefix.cpp').write_bytes((track_prefix[0] +
            '\n mainVehicle.drive(); SimOneAPI::NextFrame(frame);\n}\nreturn 0;\n}\n').encode('utf-8'))

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
        compile_run(output, ['gps_gate.cpp'], ['initial_failure', 'late_failure', 'transient_failure',
                                              'success', 'recovery'], 'gps_gate')
        compile_run(output, ['navigation.cpp'], ['waypoint_failure', 'zero_waypoints', 'route_failure',
                    'empty_route', 'short_route', 'lane_failure', 'empty_lane', 'short_lane',
                    'route_success', 'lane_success', 'repeat', 'recovery'], 'navigation')
        compile_run(output, ['lane_distance.cpp'], ['forward', 'backward', 'different_road', 'current_failure',
                    'target_failure', 'missing_lane', 'nonfinite',
                    'lateral_forward', 'lateral_backward', 'lateral_different_road', 'lateral_current_failure',
                    'lateral_target_failure', 'lateral_missing_lane', 'lateral_nonfinite'], 'lane_distance')
        compile_run(output, ['lateral_gap.cpp'], ['positive_far','negative_far','positive_near','negative_near',
                    'different_road','missing_lane','current_failure','target_failure','empty_prediction','empty_path'], 'lateral_gap')
        compile_run(drive_dir, ['main_manual.cpp', 'drive.cpp', 'following.cpp', 'track_speed.cpp', 'test_main.cpp'],
                    LOOP_TESTS + OUTPUT_TESTS, 'drive', ['/DTEST_REAL_DRIVE'])
        compile_run(follow_dir, ['main_follow.cpp', 'support.cpp', 'test.cpp'],
                    ['gps_initial', 'obstacle_initial', 'gps_transient', 'obstacle_stale',
                     'obstacle_cleared', 'both_failed', 'send_failure', 'empty_valid',
                     'gps_reverse', 'prolonged_failure', 'pid_recovery', 'route_initial', 'route_repeated'], 'follow_input')
        compile_run(follow_dir, ['track_prefix.cpp', 'support.cpp', 'test.cpp'],
                    ['gps_initial', 'obstacle_initial', 'gps_transient', 'obstacle_stale',
                     'obstacle_cleared', 'both_failed', 'send_failure', 'empty_valid',
                     'gps_reverse', 'prolonged_failure', 'route_initial', 'route_repeated'], 'track_input', ['/DTEST_TRACK_PREFIX'])
        report['passed'] = bool(report['tests']) and all(t['passed'] for t in report['tests'].values())
    except (OSError, ValueError, IndexError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        report['error'] = f'{type(error).__name__}: {error}'
    (output / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
