"""Explain a recorded formal stationary-stop run without launching CARLA.

No controller tuning or acceptance changes. Replay requires the production source
and configuration hashes recorded by the experiment to match the current files.
"""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import re

try:
    from .competition_following import FollowingController, ROOT, PARAM, read_source, extract
except ImportError:
    from competition_following import FollowingController, ROOT, PARAM, read_source, extract


def static_model(source):
    """Read only the recognized legacy expression; fail closed on source changes."""
    source = extract(source, '// 计算跟车的速度', '// 计算超车速度')
    number = r'([0-9]+(?:\.[0-9]+)?)'
    polynomial = re.search(r'float d0 = ' + number + r' \* mainVehicle.speed \* mainVehicle.speed \+ '
                           + number + r' \* mainVehicle.speed - ' + number + r';', source)
    offset = re.search(r'\(minDistance - ' + number + r'\) <= \(d0 \+ distance_expect\)', source)
    if offset is None and '(minDistance - staticReferenceOffset) <= (d0 + distance_expect)' in source:
        offset = re.search(r'return calculateFollowingSpeedWithGeometry\(obstacleList, index, ' + number + r'\);', source)
    desired = re.search(r'float distance_expect = \(index == caseStop\) \? ' + number + r' : ' + number + r';', source)
    if not all((polynomial, offset, desired)):
        raise ValueError('Static-stop expression changed; review diagnostic model extraction')
    a, b, c = map(float, polynomial.groups())
    return dict(quadratic=a, linear=b, constant=-c, reference_offset=float(offset[1]),
                stop_case_gap=float(desired[1]), ordinary_gap=float(desired[2]))


def analyze_trace(rows, final, model, desired):
    """Use pre-command observations, and keep gap reduction distinct from travel."""
    if not rows:
        raise ValueError('No recorded frames')
    fields = ('time', 'gap_m', 'speed_mps', 'longitudinal_origin_distance_m',
              'target_speed_mps', 'result_time', 'result_gap_m', 'result_speed_mps')
    samples = [{k: float(row[k]) for k in fields} for row in rows]
    for i, row in enumerate(samples):
        if not all(math.isfinite(v) for v in row.values()):
            raise ValueError('Non-finite frame data')
        if min(row['speed_mps'], row['result_speed_mps'], row['target_speed_mps']) < 0:
            raise ValueError('Negative speed in trace')
        if not 0 < row['result_time'] - row['time'] <= 0.25:
            raise ValueError('Invalid or missing sampling interval')
        following = samples[i + 1] if i + 1 < len(samples) else final
        for after, before in (('result_time', 'time'), ('result_gap_m', 'gap_m'),
                              ('result_speed_mps', 'speed_mps')):
            if not math.isfinite(float(following[before])) or abs(row[after] - float(following[before])) > 1e-5:
                raise ValueError('Frame results do not match the next observation')
    stopped_indices = [i for i, row in enumerate(samples) if row['target_speed_mps'] == 0]
    starts = [i for i in stopped_indices if i == 0 or samples[i-1]['target_speed_mps'] != 0]
    restarts = sum(a['target_speed_mps'] == 0 and b['target_speed_mps'] > 0
                   for a, b in zip(samples, samples[1:]))
    result = dict(stop_request_episodes=len(starts), target_restart_count=restarts,
                  final_gap_m=float(final['gap_m']), final_speed_mps=float(final['speed_mps']),
                  diagnostic_scope='stationary lead, straight corridor; gap change includes bounding-box rotation',
                  decomposition_available=False)
    if not starts:
        result['reason'] = 'No zero-speed request was recorded'
        return result
    first = samples[starts[0]]
    result['first_zero_request'] = dict(elapsed_seconds=first['time'] - samples[0]['time'],
        body_gap_m=first['gap_m'], speed_mps=first['speed_mps'],
        reference_distance_m=first['longitudinal_origin_distance_m'])
    if restarts or samples[-1]['target_speed_mps'] != 0 or float(final['speed_mps']) > 0.1:
        result['reason'] = 'Requires one uninterrupted zero-speed episode ending at standstill'
        return result
    speed = first['speed_mps']
    predicted = model['quadratic'] * speed * speed + model['linear'] * speed + model['constant']
    geometry = first['longitudinal_origin_distance_m'] - first['gap_m']
    supplied_offset = rows[starts[0]].get('static_reference_offset_m', '')
    applied_offset = float(supplied_offset) if supplied_offset != '' else model['reference_offset']
    if not math.isfinite(applied_offset) or applied_offset < 0:
        raise ValueError('Invalid applied geometry offset')
    threshold_crossing = applied_offset + desired + predicted - first['longitudinal_origin_distance_m']
    reduction = first['gap_m'] - float(final['gap_m'])
    mismatch = geometry - applied_offset
    residual = reduction - predicted
    shortfall = desired - float(final['gap_m'])
    result.update(decomposition_available=True, desired_gap_m=desired,
        measured_reference_to_body_offset_m=geometry,
        legacy_reference_offset_m=model['reference_offset'],
        applied_reference_offset_m=applied_offset,
        geometry_mismatch_m=mismatch,
        threshold_crossing_m=threshold_crossing,
        previous_frame_requested_motion=starts[0] > 0,
        predicted_braking_distance_m=predicted,
        post_request_gap_reduction_m=reduction,
        braking_model_residual_m=residual,
        desired_gap_shortfall_m=shortfall,
        decomposition_residual_m=shortfall - (mismatch + threshold_crossing + residual),
        interpretation='Algebraic trace attribution, not independent causal calibration. Threshold crossing '
                       'is sampling-related only when an earlier valid frame requested motion. Braking '
                       'residual includes actuator response, dynamics, sampling and box rotation.')
    return result


def analyze_run(directory):
    report_path, csv_path = directory / 'report.json', directory / 'frames.csv'
    report = json.loads(report_path.read_text(encoding='utf-8'))
    if report['parameters'].get('scenario', 'stationary') != 'stationary' or report['parameters']['controller'] != 'formal-follow':
        raise ValueError('Only formal stationary-lead runs are supported')
    if report.get('collisions') or report.get('cleanup_errors'):
        raise ValueError('Run has collision or cleanup errors; inspect original report')
    if report.get('max_lateral_m', math.inf) > 0.4:
        raise ValueError('Run left the supported corridor')
    for relative in ('competition/TrajectoryControl/src/core/obstacle.cpp',
                     'competition/TrajectoryControl/src/tools/controller.hpp'):
        digest = hashlib.sha256((ROOT / relative).read_bytes()).hexdigest()
        if digest != report['following_build']['source_sha256'][relative]:
            raise ValueError(f'Production source no longer matches recorded run: {relative}')
    if hashlib.sha256(PARAM.read_bytes()).hexdigest() != report['following_parameter_sha256']:
        raise ValueError('Configuration no longer matches recorded run')
    with csv_path.open(encoding='utf-8', newline='') as stream:
        rows = list(csv.DictReader(stream))
    model = static_model(read_source((ROOT / 'competition/TrajectoryControl/src/core/obstacle.cpp').read_bytes()))
    desired = model['stop_case_gap'] if report['case_index'] == report['following_parameters']['caseStop'] else model['ordinary_gap']
    result = analyze_trace(rows, report['final'], model, desired)
    with FollowingController() as controller:
        for i, row in enumerate(rows):
            for field, limit in (('lead_speed_mps', 0), ('lead_displacement_m', 0.05),
                                 ('lateral_m', 0.4), ('yaw_error_deg', 3)):
                value = float(row[field])
                if not math.isfinite(value) or abs(value) > limit:
                    raise ValueError(f'Unsupported stationary/corridor state at row {i}: {field}')
            logged = float(row['formal_target_speed_mps'])
            if not math.isfinite(logged):
                raise ValueError(f'Non-finite formal output at row {i}')
            geometry = None
            if report['parameters'].get('distance_geometry', 'legacy') == 'body':
                geometry = float(row['static_reference_offset_m'])
                measured = float(row['longitudinal_origin_distance_m']) - float(row['gap_m'])
                if not math.isfinite(geometry) or abs(geometry - measured) > 1e-5:
                    raise ValueError(f'Applied geometry does not match measured body gap at row {i}')
            elif row.get('static_reference_offset_m', '') != '':
                raise ValueError('Legacy run unexpectedly records an explicit geometry input')
            replay = controller.step(0, float(row['speed_mps']),
                [(float(row['longitudinal_origin_distance_m']), 0, 0, 1, 1)], report['case_index'],
                static_reference_offset=geometry)
            if abs(replay['target_speed'] - logged) > 1e-5:
                raise ValueError(f'Formal command replay mismatch at row {i}')
            if abs(float(row['target_speed_mps']) - logged) > 1e-5:
                raise ValueError(f'Applied target differs from formal output at row {i}')
    result.update(formal_replay_frames=len(rows), production_source_matched=True,
                  original_acceptance_passed=report['passed'], competition_verified=False,
                  source_report_sha256=hashlib.sha256(report_path.read_bytes()).hexdigest(),
                  source_csv_sha256=hashlib.sha256(csv_path.read_bytes()).hexdigest(),
                  model=model)
    return result


def save_analysis(directory):
    analysis = analyze_run(directory)
    (directory / 'stop-analysis.json').write_text(json.dumps(analysis, indent=2), encoding='utf-8')
    return analysis


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path, help='Existing experiment report/CSV directory')
    args = parser.parse_args()
    analysis = save_analysis(args.directory)
    output = args.directory / 'stop-analysis.json'
    print(json.dumps(analysis, indent=2))
    print(f'Analysis: {output.resolve()}')
