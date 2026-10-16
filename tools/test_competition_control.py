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


RAW_OBSTACLE_TESTS = [
    f'raw_{field}_{kind}'
    for field in ('posX', 'posY', 'posZ', 'velX', 'velY', 'velZ', 'oriZ', 'width', 'length')
    for kind in ('nan', 'inf', 'neginf')
] + ['raw_initial', 'raw_repeated', 'raw_second', 'raw_unused', 'raw_signed']

OBSTACLE_HEADING_TESTS = ['obstacle_heading_' + kind for kind in
    ('failed', 'zero', 'nan', 'inf', 'empty', 'initial', 'repeated', 'static', 'reverse', 'send_failure', 'pid')]

OBSTACLE_PROJECTION_TESTS = [f'projection_{group}_{stage}_{mode}'
    for group in ('raw', 'group') for stage in range(1, 4) for mode in range(1, 6)
] + [f'projection_{group}_3_{mode}' for group in ('raw', 'group') for mode in (6, 7)
] + ['projection_initial', 'projection_repeated', 'projection_crossroad_valid', 'projection_crossroad_failure']

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
    'output_arbitration_bad_light',
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
        'util/UtilDriver.h', 'util/UtilMath.h',
        'TrajectoryControl/src/main.cpp', 'TrajectoryControl/src/main_follow.cpp',
        'TrajectoryControl/src/main_track.cpp',
        'TrajectoryControl/src/core/obstacle.h', 'TrajectoryControl/src/tools/prediction.cpp',
        'TrajectoryControl/src/core/stopline.cpp', 'TrajectoryControl/src/core/stopline.h',
    )
    report = {'sdk_build_verified': False, 'platform_run_verified': False,
              'official_score_verified': False,
              'scope': 'geometry; full vehicle update and following loop; track input prefix; strategy loop; obstacle fetch; real drive/pursuit/PID/curvature',
              'limitations': 'map/API/frame-input fixtures; following/track input suites substitute drive and convert legacy fatal ASSERT to test failure; no full track planner, SDK, vehicle dynamics or scoring',
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
            root / 'tests/cpp/traffic_light_test.cpp',
            root / 'tests/cpp/obstacle_boundary_test.cpp',
            root / 'tests/cpp/road_width_test.cpp',
            root / 'tests/cpp/obstacle_constructor_test.cpp',
            root / 'tests/cpp/constructed_follow_test.cpp',
            root / 'tests/cpp/grouped_constructor_test.cpp',
            root / 'tests/cpp/stopline_projection_test.cpp',
            root / 'tests/cpp/stopline_drive_test.cpp',
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
        width_harness = read_source(root / 'tests/cpp/road_width_test.cpp')
        width_harness = width_harness.replace('// @ROAD_WIDTH@', between(
            sources['TrajectoryControl/src/core/process.cpp'], 'double getRoadWidth(', 'float getLaneAzimuth('))
        constructors = obstacle.split('/* 计算相对于主车的 ST 坐标 */')[1:]
        for marker, part in zip(('// @RAW_WIDTH@', '// @GROUP_WIDTH@'), constructors):
            start = part.index('SimOneAPI::GetLaneWidth(')
            # Include the complete statement/validation block, starting at its line.
            start = part.rfind('\n', 0, start) + 1
            if 'laneWidth = std::numeric_limits' in part[:start]:
                start = part.rfind('laneWidth = std::numeric_limits', 0, start)
            end = part.index('roadWidth = getRoadWidth(', start)
            end = part.index(';', end) + 1
            width_harness = width_harness.replace(marker, part[start:end])
        (output / 'road_width.cpp').write_bytes(width_harness.encode('utf-8'))
        boundary = between(obstacle, 'void getObstacleLaneSidePosition(', '// 可以用 ST 坐标')
        boundary_harness = read_source(root / 'tests/cpp/obstacle_boundary_test.cpp')
        checked_lane = (between(obstacle, 'static SSD::SimString getObstacleLaneChecked(', 'void getObstacleLaneSidePosition(')
                        if 'static SSD::SimString getObstacleLaneChecked(' in obstacle else '')
        boundary_harness = boundary_harness.replace('// @CHECKED_LANE@', checked_lane)
        road_boundary = between(obstacle, 'void getObstacleRoadSidePosition(', 'bool isObstacleAmongCrosswalk(')
        boundary_harness = boundary_harness.replace('// @ROAD_BOUNDARY@', road_boundary)
        (output / 'boundary.cpp').write_bytes(boundary_harness.replace('// @BOUNDARY@', boundary).encode('utf-8'))
        constructor_fixture = read_source(root / 'tests/cpp/obstacle_constructor_test.cpp')
        replacements_ctor = {
            '// @CLASS@': between(sources['TrajectoryControl/src/core/obstacle.h'], 'class Obstacle {', 'extern SSD::SimPoint3D calculateObstacleTL('),
            '// @CORNERS@': between(obstacle, 'SSD::SimPoint3D calculateObstacleTL(', 'void groupObstacleByDist('),
            '// @PROCESS@': between(sources['TrajectoryControl/src/core/process.cpp'], 'double getRoadWidth(', 'bool getValidTrafficLight('),
            '// @AZIMUTH@': between(sources['TrajectoryControl/src/core/process.h'], 'inline float calculateResultantAzimuth(', '/**'),
            '// @SPEED@': between(sources['util/UtilMath.h'], 'static double calculateSpeed(const double velX, const double velY, const double velZ)', '\n\t//'),
            '// @PREDICTION@': sources['TrajectoryControl/src/tools/prediction.cpp'].split('#include "logger.hpp"', 1)[1],
            '// @CONSTRUCTOR@': between(obstacle, 'Obstacle::Obstacle(const SimOne_Data_Obstacle_Entry& obstacle)', '// 通过索引来构造'),
            '// @MERGED_CONSTRUCTOR@': between(obstacle, 'Obstacle::Obstacle(const std::vector<size_t>& index)', 'bool calculateNearestObstacle('),
        }
        for marker, code in replacements_ctor.items():
            constructor_fixture = constructor_fixture.replace(marker, code)
        preamble = boundary_harness.replace('// @BOUNDARY@', boundary).split('int main(', 1)[0]
        (output / 'constructor.cpp').write_bytes((preamble + constructor_fixture).encode('utf-8'))
        stopline = sources['TrajectoryControl/src/core/stopline.cpp']
        stop_fixture = read_source(root / 'tests/cpp/stopline_projection_test.cpp')
        stop_fixture = stop_fixture.replace('// @CLASS@', between(sources['TrajectoryControl/src/core/stopline.h'],
            'class StopLine {', '// 计算到主车最近的停止线'))
        stop_fixture = stop_fixture.replace('// @CONSTRUCTOR@', between(stopline,
            'StopLine::StopLine(Obstacle& obstacle)', 'StopLine::StopLine(const ManualStopLineReservoir&'))
        stop_helpers = (between(stopline, 'static bool readStopLineEndpoints(', 'bool calculateNearestStopLine(')
                        if 'static bool readStopLineEndpoints(' in stopline else '')
        stop_fixture = stop_fixture.replace('// @LIGHT_CONSTRUCTOR@', between(stopline,
            'StopLine::StopLine(const HDMapStandalone::MSignal& light)', 'StopLine::StopLine(Obstacle& obstacle)'))
        stop_fixture = stop_fixture.replace('double StopLine::calculateVehicleDistance', stop_helpers + '\ndouble StopLine::calculateVehicleDistance', 1)
        stop_fixture = stop_fixture.replace('// @PROJECTION@', between(stopline,
            'bool StopLine::checkVechicleInSameLane(', 'bool getLaneChangePath(StopLine& stopLine)'))
        (output / 'stopline_projection.cpp').write_bytes(stop_fixture.encode('utf-8'))
        grouped = read_source(root / 'tests/cpp/grouped_constructor_test.cpp')
        grouped = grouped.replace('// @DISTANCE@', between(sources['util/UtilMath.h'],
            'static double distance(double pt1_x,', 'static double planarDistance('))
        grouped = grouped.replace('// @GROUPING@', between(obstacle,
            'void groupObstacleByDist(', 'void groupObstacleByS('))
        (output / 'grouped_constructor.cpp').write_bytes(('#define TEST_OBSTACLE_CAPACITY 6\n'
            + preamble + constructor_fixture.split('int main(', 1)[0] + grouped).encode('utf-8'))
        constructed_follow = read_source(root / 'tests/cpp/constructed_follow_test.cpp')
        integration_parts = {
            '// @CONTROLLER@': sources['TrajectoryControl/src/tools/controller.hpp'],
            '// @DRIVE@': between(sources['TrajectoryControl/src/core/vehicle.cpp'],
                'void MainVehicle::drive(void)', '// 清除所有邻域信息').replace('MainVehicle::drive', 'MainVehicleFixture::drive', 1),
            '// @FETCH@': 'bool updateObstacleData(' + sources['TrajectoryControl/src/main.cpp'].split('bool updateObstacleData(', 1)[1],
            '// @NEAREST@': between(obstacle, 'bool calculateNearestObstacle(', '// 找到所有在主车后方'),
            '// @FOLLOWING@': between(obstacle, 'static float calculateFollowingSpeedImpl(', '// 计算超车速度'),
            '// @LOOP@': sources['TrajectoryControl/src/main_follow.cpp'].replace('#include "main.h"', ''),
        }
        for marker, code in integration_parts.items():
            constructed_follow = constructed_follow.replace(marker, code)
        (output / 'constructed_follow.cpp').write_bytes((preamble + constructor_fixture.split('int main(', 1)[0]
            + constructed_follow).encode('utf-8'))
        corners = between(obstacle, 'SSD::SimPoint3D calculateObstacleTL(', 'void groupObstacleByDist(')
        merge = between(obstacle, '        std::vector<SSD::SimPoint3D> tlList, brList;',
                        '        laneID = getObstacleLaneChecked(pt);' if checked_lane else
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
        light_code = between(sources['TrajectoryControl/src/core/process.cpp'], 'bool getValidTrafficLight(', '// 判断主车轨迹是否涉及变道过程')
        light_call = 'getValidTrafficLight(list,light,known)' if 'bool& dataValid' in light_code else 'getValidTrafficLight(list,light)'
        light_harness = read_source(root / 'tests/cpp/traffic_light_test.cpp')
        (output / 'traffic_light.cpp').write_bytes(('#define READ_LIGHT(list,light,known) '+light_call+'\n'+
            light_harness.replace('// @TRAFFIC_LIGHT@',light_code)).encode('utf-8'))

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
            '// @PRODUCTION_FOLLOWING@': between(obstacle, '// 计算跟车的速度', '// 计算超车速度'),
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
        stop_drive = stop_fixture.split('int main(', 1)[0]
        stop_drive = stop_drive.replace(
            'double StopLine::calculateVehicleDistance(const SSD::SimPoint3D&) const { return 20; }',
            between(stopline, 'double StopLine::calculateVehicleDistance(', 'bool StopLine::isStopLineBehindVehicle('))
        stop_drive_body = read_source(root / 'tests/cpp/stopline_drive_test.cpp')
        stop_drive_parts = {
            '// @CONTROLLER@': support,
            '// @DEFAULT_AND_NEAREST@': between(stopline, 'bool calculateNearestStopLine(',
                                               'StopLine::StopLine(const HDMapStandalone::MSignal& light)'),
            '// @DRIVE@': drive.replace('MainVehicle::drive', 'Vehicle::drive', 1),
            '// @ARBITRATION@': speed_support.replace('#include "main.h"', ''),
            '// @BRAKING@': between(track,
                'if (potentialStopLine.type == StopLine::Type::Whole &&', '/* 半停止线，主车的策略是变道 */'),
        }
        for marker, code in stop_drive_parts.items():
            stop_drive_body = stop_drive_body.replace(marker, code)
        (output / 'stopline_drive.cpp').write_bytes((stop_drive + stop_drive_body).encode('utf-8'))

        follow_dir = output / 'follow_input'
        follow_dir.mkdir(exist_ok=True)
        for name in ('main.h', 'test.cpp'):
            shutil.copyfile(root / 'tests/cpp/follow_input_stub' / name, follow_dir / name)
        # Execute exact projection sections from both production constructors.
        # Geometry, widths, prediction and classification remain outside this fixture.
        projection_parts = obstacle.split('/* 计算相对于主车的 ST 坐标 */')[1:]
        if len(projection_parts) != 2:
            raise ValueError('review obstacle constructor projection extraction')
        fixture = read_source(follow_dir / 'main.h')
        for marker, part in zip(('// @RAW_PROJECTION@', '// @GROUP_PROJECTION@'), projection_parts):
            width_start = ('laneWidth = std::numeric_limits' if 'laneWidth = std::numeric_limits' in part
                           else 'SimOneAPI::GetLaneWidth(')
            fixture = fixture.replace(marker, part.split(width_start, 1)[0])
        (follow_dir / 'main.h').write_bytes(fixture.encode('utf-8'))
        (follow_dir / 'controller.hpp').write_bytes(support.encode('utf-8'))
        (follow_dir / 'main_follow.cpp').write_bytes(sources['TrajectoryControl/src/main_follow.cpp'].encode('utf-8'))
        fetch = sources['TrajectoryControl/src/main.cpp'].split('bool updateObstacleData(', 1)[1]
        update = between(sources['TrajectoryControl/src/core/vehicle.cpp'],
                         'bool MainVehicle::update(', '// 必须在本帧 obstacleList 构建完成后调用')
        azimuth = between(sources['TrajectoryControl/src/core/process.cpp'],
                          'float getLaneAzimuth(', 'bool getValidTrafficLight(')
        speed = between(sources['util/UtilMath.h'],
                        'static double calculateSpeed(const double velX, const double velY)\n',
                        '\n\t//').replace('static double calculateSpeed(', 'double UtilMath::calculateSpeed(', 1)
        follow_support = '#include "main.h"\n' + speed + '\n' + azimuth + '\n' + update
        follow_support += 'bool updateObstacleData(' + fetch + '\n'
        follow_support += replacements['// @PRODUCTION_NEAREST@'] + replacements['// @PRODUCTION_FOLLOWING@']
        (follow_dir / 'support.cpp').write_bytes(follow_support.encode('utf-8'))
        # Execute the real track input gate and obstacle-construction loops.
        # Obstacle internals/grouping remain fixtures; downstream planning is drive().
        track_prefix = sources['TrajectoryControl/src/main_track.cpp'].split(
            '\t\tFlagType::isCrosswalkExist = getCrossWalk();', 1)
        if len(track_prefix) != 2:
            raise ValueError('missing track input boundary')
        construction = between(track, '\t\tstd::vector<size_t> staticObstacleIndex;', '\t\tfloat groupingSThres')
        (follow_dir / 'track_prefix.cpp').write_bytes((track_prefix[0] +
            '\n obstacleList.clear(); stopLineList.clear();\n' + construction +
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
        compile_run(output, ['stopline_drive.cpp'], ['near','far','ignored']
                    + [f'{kind}_{mode}' for kind in ('light','crosswalk','projection')
                       for mode in ('forward','reverse','slide','both')], 'stopline_drive')
        compile_run(output, ['stopline_projection.cpp'], ['valid','outside','reversed']
                    + [f'{fault}_{call}' for fault in ('unwritten','written','nan_s','nan_t','inf_z')
                       for call in range(3)]
                    + [f'{kind}_{fault}' for kind in ('light','crosswalk')
                       for fault in ('valid','empty','knots0','knots1','nan','inf','degenerate')], 'stopline_projection')
        compile_run(output, ['grouped_constructor.cpp'],
                    ['exact','below','above','chain','sparse','reordered','empty','recovery'], 'grouped_constructor')
        compile_run(output, ['constructed_follow.cpp'], ['valid','prediction_nan','prediction_inf','prediction_growth',
                    'near_failed_written','heading_failed','projection_failed','road_projection_failed','width_failed',
                    'failed_partial','link_failed','left_empty','right_empty','initial','repeated','reverse','send_failure',
                    'slide','reverse_slide','speed_cap','steering_nan'], 'constructed_follow')
        compile_run(output, ['constructor.cpp'], ['valid','static','vertical','near_failed_written','heading_failed',
                    'projection_failed','road_projection_failed','width_failed','failed_partial','link_failed',
                    'left_empty','right_empty','prediction_nan','prediction_inf','prediction_growth',
                    'merged_valid','merged_single','merged_empty','merged_near_failed_written','merged_heading_failed',
                    'merged_projection_failed','merged_road_projection_failed','merged_width_failed','merged_failed_partial',
                    'merged_link_failed','merged_left_empty','merged_right_empty']
                    + [f'axes_{ego}_{lane}' for ego in (0,90,180,270) for lane in (0,90,180,270)], 'obstacle_constructor')
        compile_run(output, ['road_width.cpp'],
                    [f'{kind}_{mode}' for kind in ('road','raw','group') for mode in
                     ('valid','single','failed','partial','unwritten','nan','inf','negative','zero','link_failed')]
                    + ['road_empty','road_point_nan','road_overflow']
                    + [f'{kind}_{mode}' for kind in ('raw','group') for mode in
                       ('first_failed','first_unwritten','first_nan')], 'road_width')
        compile_run(output, ['boundary.cpp'], ['valid','empty_lane','failed_partial','left_empty',
                    'right_empty','nan','inf','all_nan','point_nan',
                    'road_valid','road_single','road_aligned','road_empty_lane','road_failed_partial','road_right_failed',
                    'road_left_empty','road_right_empty','road_nan','road_point_nan','road_link_failed','road_cycle',
                    'near_failed_written','near_failed_empty','road_near_failed_written','road_near_failed_empty'], 'obstacle_boundary')
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
        compile_run(output, ['traffic_light.cpp'], ['red','yellow','green','failure','green_then_failure',
                    'failure_then_red','unrelated_red','empty'], 'traffic_light')
        compile_run(drive_dir, ['main_manual.cpp', 'drive.cpp', 'following.cpp', 'track_speed.cpp', 'test_main.cpp'],
                    LOOP_TESTS + OUTPUT_TESTS, 'drive', ['/DTEST_REAL_DRIVE'])
        compile_run(follow_dir, ['main_follow.cpp', 'support.cpp', 'test.cpp'],
                    ['gps_initial', 'obstacle_initial', 'gps_transient', 'obstacle_stale',
                     'obstacle_cleared', 'both_failed', 'send_failure', 'empty_valid',
                     'gps_reverse', 'prolonged_failure', 'pid_recovery', 'route_initial', 'route_repeated',
                     'map_nearest','map_projection','map_empty_lane','map_nonfinite','map_bad_position',
                     'map_initial','map_repeated','heading_failed','heading_nonfinite','heading_zero',
                     'link_partial','link_cleared','link_initial','link_repeated','road_invalid','road_overflow',
                     'successor_ambiguous','link_empty_valid','junction_valid',
                     'count_negative','count_overflow','count_extreme','count_initial','count_repeated','count_capacity'] + RAW_OBSTACLE_TESTS + OBSTACLE_HEADING_TESTS + OBSTACLE_PROJECTION_TESTS, 'follow_input')
        compile_run(follow_dir, ['track_prefix.cpp', 'support.cpp', 'test.cpp'],
                    ['gps_initial', 'obstacle_initial', 'gps_transient', 'obstacle_stale',
                     'obstacle_cleared', 'both_failed', 'send_failure', 'empty_valid',
                     'gps_reverse', 'prolonged_failure', 'route_initial', 'route_repeated',
                     'map_nearest','map_projection','map_empty_lane','map_nonfinite','map_bad_position',
                     'map_initial','map_repeated','heading_failed','heading_nonfinite','heading_zero',
                     'link_partial','link_cleared','link_initial','link_repeated','road_invalid','road_overflow',
                     'successor_ambiguous','link_empty_valid','junction_valid',
                     'count_negative','count_overflow','count_extreme','count_initial','count_repeated','count_capacity'] + RAW_OBSTACLE_TESTS + OBSTACLE_HEADING_TESTS + OBSTACLE_PROJECTION_TESTS, 'track_input', ['/DTEST_TRACK_PREFIX'])
        report['passed'] = bool(report['tests']) and all(t['passed'] for t in report['tests'].values())
    except (OSError, ValueError, IndexError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        report['error'] = f'{type(error).__name__}: {error}'
    (output / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
