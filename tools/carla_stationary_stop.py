"""Follow a stationary, braking or stop-go lead on a verified straight Town01 lane.

Exclusive local CARLA server required: loads Town01 if necessary.
Default: compile and run production nearest-obstacle and following-speed bodies.
Uses ground-truth straight-lane state. SDK Speed-mode actuator mapping is CARLA-specific.
"""

import argparse
import csv
from datetime import datetime
import hashlib
import json
import math
from pathlib import Path
import queue
import time

if __package__:
    from .competition_stop import StopController
    from .competition_following import FollowingController
    from .lead_profile import StopGoProfile, BrakingProfile, assess_stop_go, assess_braking
    from .competition_steering import SteeringController, carla_to_formal
else:
    from competition_stop import StopController
    from competition_following import FollowingController
    from lead_profile import StopGoProfile, BrakingProfile, assess_stop_go, assess_braking
    from competition_steering import SteeringController, carla_to_formal


def speed_actuators(target, speed):
    """Translate m/s target to CARLA pedals; never change the target itself.

    This feedback controller substitutes SimOne Speed mode, not vehicle planning.
    It is an experimental mapping, not a model of SimOne's internal controller.
    """
    if not all(math.isfinite(v) and v >= 0 for v in (target, speed)):
        raise ValueError('Invalid speed target or measurement')
    if target == 0:
        return 0.0, 1.0
    error = target - speed
    if error > 0:
        return min(0.55, 0.08 + 0.35 * error), 0.0
    return 0.0, min(1.0, -0.45 * error)


def observed_dwell(samples, stop_gap, max_gap=0.25):
    """Measure logged standstill; stop_gap=None measures it regardless of location."""
    end = start = None
    for state in reversed(samples):
        if (not all(math.isfinite(state[key]) for key in ('time', 'speed_mps', 'gap_m')) or
                abs(state['speed_mps']) > 0.1 or
                (stop_gap is not None and abs(state['gap_m'] - stop_gap) > 0.5)):
            break
        stamp = state['time']
        if not math.isfinite(stamp) or (start is not None and not 0 <= start - stamp <= max_gap):
            break
        if end is None:
            end = stamp
        start = stamp
    return 0.0 if end is None else end - start


def projected_gap(ego_points, lead_points, axis):
    """Signed separation of body boxes along a fixed unit road direction."""
    def project(point):
        return sum(a * b for a, b in zip(point, axis))
    return min(map(project, lead_points)) - max(map(project, ego_points))


def reference_to_body_offset(ego_points, lead_points, ego_origin, lead_origin, axis):
    """Projected ego front + lead rear extent, allowing non-centred origins.

    Only supports the straight, common-axis experiment. No claim about curved
    lane-s, sensor reference points or SimOne obstacle geometry is made here.
    """
    vectors = [ego_origin, lead_origin, axis, *ego_points, *lead_points]
    if (not ego_points or not lead_points or any(len(v) != 3 for v in vectors)
            or not all(math.isfinite(x) for v in vectors for x in v)
            or abs(sum(x*x for x in axis) - 1) > 1e-6):
        raise ValueError('Invalid geometry or non-unit projection axis')
    def relative(point, origin):
        return sum((p-o)*a for p, o, a in zip(point, origin, axis))
    offset = max(relative(p, ego_origin) for p in ego_points) - min(relative(p, lead_origin) for p in lead_points)
    if offset < 0:
        raise ValueError('Unsupported negative reference-to-body offset')
    return offset


def stop_command(gap, speed, cruise, stop_gap):
    """Distance envelope followed by proportional speed feedback (SI units)."""
    if not all(math.isfinite(v) for v in (gap, speed, cruise, stop_gap)):
        raise ValueError('Control inputs must be finite')
    if speed < 0 or cruise <= 0 or stop_gap <= 0:
        raise ValueError('Invalid speed or distance setting')
    remaining = max(0.0, gap - stop_gap)
    target = min(cruise, math.sqrt(2 * 1.5 * remaining), 0.8 * remaining)
    if remaining <= 0.15 and speed < 0.25:
        return 0.0, 0.0, 1.0, True
    error = target - speed
    if error > 0:
        return target, min(0.55, 0.08 + 0.35 * error), 0.0, False
    return target, 0.0, min(1.0, -0.45 * error), False


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scenario', choices=('stationary', 'stop-go', 'lead-brake'), default='stationary')
    parser.add_argument('--steering', choices=('formal', 'none'), default='formal',
                        help='Production pursuit/PID, or zero steering for earlier experiment comparison')
    parser.add_argument('--distance-geometry', choices=('body', 'legacy'), default='body',
                        help='Measured CARLA projected body geometry, or inherited SimOne fixed offset')
    parser.add_argument('--controller', choices=('formal-follow', 'experimental'), default='formal-follow',
                        help='Production following code by default; experimental retains the old Python planner')
    parser.add_argument('--following-policy', choices=('scenario', 'legacy'),
                        help='Default scenario for lead-brake, otherwise legacy; legacy reproduces old 45m reference target')
    parser.add_argument('--speed-kmh', type=float, help='Default 30 for lead-brake; otherwise 10')
    parser.add_argument('--gap', type=float, help='Initial body gap in m; default 50 for lead-brake, otherwise 25')
    parser.add_argument('--stop-gap', type=float, default=2.0, help='Desired final body gap in m')
    parser.add_argument('--timeout', type=float, default=60.0, help='Simulation seconds')
    parser.add_argument('--dwell-seconds', type=float, default=2.0, help='Continuous observed standstill in simulation seconds')
    parser.add_argument('--exclusive-dwell', action='store_true', help='Require strictly more than dwell-seconds')
    parser.add_argument('--fixed-delta-seconds', type=float, default=0.05, help='Simulation step, 0.02..0.1 seconds')
    parser.add_argument('--no-realtime', action='store_true', help='Run without wall-clock pacing')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.speed_kmh is None:
        args.speed_kmh = 30.0 if args.scenario == 'lead-brake' else 10.0
    if args.gap is None:
        args.gap = 50.0 if args.scenario == 'lead-brake' else 25.0
    if args.following_policy is None:
        args.following_policy = 'scenario' if args.scenario == 'lead-brake' else 'legacy'
    if args.following_policy == 'scenario' and (args.scenario != 'lead-brake' or args.distance_geometry != 'body'):
        parser.error('Explicit scenario policy currently requires lead-brake and measured body geometry')
    if not all(math.isfinite(v) for v in (args.speed_kmh, args.gap, args.stop_gap, args.timeout,
                                         args.dwell_seconds, args.fixed_delta_seconds)):
        parser.error('Numeric arguments must be finite')
    if not 1 <= args.speed_kmh <= 30 or not 1 <= args.stop_gap <= 5:
        parser.error('Supported speed: 1..30 km/h; stop gap: 1..5 m')
    if not args.stop_gap + 5 <= args.gap <= 65 or not 5 <= args.timeout <= 120:
        parser.error('Initial gap: stop-gap+5..65 m; timeout: 5..120 s')
    if not 0 < args.dwell_seconds < args.timeout or not 0.02 <= args.fixed_delta_seconds <= 0.1:
        parser.error('Dwell must be >0 and <timeout; simulation step must be 0.02..0.1 s')
    if args.scenario in ('stop-go', 'lead-brake'):
        if args.controller != 'formal-follow':
            parser.error('Moving lead scenarios require formal-follow')
        profile_type = BrakingProfile if args.scenario == 'lead-brake' else StopGoProfile
        if args.timeout < profile_type(speed=args.speed_kmh / 3.6).end_time:
            parser.error('Timeout must cover the complete lead profile')
    if args.output is None:
        args.output = Path('work/carla') / datetime.now().strftime('stationary-stop-%Y%m%d-%H%M%S-%f')
    if args.output.exists() and (not args.output.is_dir() or any(args.output.iterdir())):
        parser.error('Output directory must be empty or new')
    return args


def run(args):
    import carla

    args.output.mkdir(parents=True, exist_ok=True)
    actors, rows, collisions = [], [], []
    world = original = None
    shared = None
    following = None
    steering = None
    dt = args.fixed_delta_seconds
    profile = StopGoProfile(speed=args.speed_kmh / 3.6) if args.scenario == 'stop-go' else None
    if args.scenario == 'lead-brake':
        profile = BrakingProfile(speed=args.speed_kmh / 3.6)
    policy = (dict(moving_gap_m=args.gap, stopped_gap_m=args.stop_gap, max_speed_mps=args.speed_kmh/3.6)
              if args.following_policy == 'scenario' else None)
    report = dict(passed=False, competition_verified=False, input_source='CARLA ground truth',
                  parameters={k: str(v) if isinstance(v, Path) else v for k, v in vars(args).items()},
                  source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    client = carla.Client('127.0.0.1', 2000)
    client.set_timeout(10.0)
    try:
        shared = StopController(args.dwell_seconds, exclusive=args.exclusive_dwell)
        report.update(controller='competition/util/StopController.h via C ABI',
                      controller_build=shared.build,
                      acceptance=dict(stationary_speed_mps=0.1, gap_error_m=0.5,
                                      dwell_seconds=args.dwell_seconds, exclusive=args.exclusive_dwell,
                                      scope='auxiliary CARLA experiment, not official scoring'))
        if args.controller == 'formal-follow':
            following = FollowingController()
            report.update(controller='production calculateNearestObstacle + caculateFollowingSpeed',
                          following_build=following.build, following_parameters=following.parameters,
                          following_parameter_sha256=following.parameter_sha256,
                          case_index=2,
                          stop_core_role='observer only: never overrides production speed',
                          input_mapping='straight lane: ego s=0; obstacle s=projected actor-origin separation; static, same road/lane',
                          limitations='no production Obstacle constructor, map queries, main_follow loop, drive(), lateral control or SimOne SDK')
            if args.scenario == 'stop-go':
                report.update(case_index=following.parameters['caseStop'],
                              lead_profile=vars(profile),
                              input_mapping='straight lane actor-origin separation; analytic lead speed; static below 0.01 m/s, otherwise same-direction',
                              acceptance=dict(gap_m=[10, 50], standstill_seconds=2, recovery_speed_mps=1,
                                  scope='auxiliary stop-go stimulus; fixed hold, not official reactive restart sequence'),
                              lead_profile_sha256=hashlib.sha256(Path(__file__).with_name('lead_profile.py').read_bytes()).hexdigest())
            elif args.scenario == 'lead-brake':
                report.update(lead_profile=vars(profile), following_policy=policy,
                              input_mapping='straight lane actor-origin separation; kinematic lead speed; static below 0.01 m/s, otherwise same-direction',
                              case_index_note=('Explicit policy bypasses case index; no official caseName mapping' if policy else
                                               'Inherited non-caseStop branch (2m target), not verified 2027 platform caseName'),
                              rule_reference='2027 scoring PDF physical p11 table4: 30km/h, 50m, cruise 3s then lead deceleration 2m/s^2; 1..3.5m stop gap deducts 50 points',
                              acceptance=dict(stop_gap_m=args.stop_gap, gap_error_m=0.5, stationary_speed_mps=0.1,
                                  dwell_seconds=args.dwell_seconds, exclusive=args.exclusive_dwell,
                                  cruise_speed_error_mps=0.5, cruise_gap_error_m=0.5,
                                  scope='engineering stop AND cruise checks, not official scoring; 50m interpreted as body gap; fixed 30s observation after lead stop'),
                              lead_profile_sha256=hashlib.sha256(Path(__file__).with_name('lead_profile.py').read_bytes()).hexdigest())
        if args.steering == 'formal':
            steering = SteeringController()
            report.update(steering_build=steering.build, steering_parameters=steering.parameters,
                          steering_parameter_sha256=steering.parameter_sha256,
                          steering_scope='production pursuit + PID, fixed following-mode gains; no drive, curvature gain or lane-change offset',
                          steering_mapping='CARLA (x,y,yaw_deg) -> (x,-y,-yaw_rad); normalized command clamped to [-1,1]; actuator calibration not verified')
        report.update(client_version=client.get_client_version(), server_version=client.get_server_version())
        if report['client_version'] != '0.9.15' or report['server_version'] != '0.9.15':
            raise RuntimeError('This experiment requires CARLA client and server 0.9.15')
        world = client.get_world()
        if any(actor.type_id.startswith(('vehicle.', 'walker.', 'sensor.')) for actor in world.get_actors()):
            raise RuntimeError('Server is occupied; finish other experiments before running this one')
        # Reuse an empty Town01 to avoid repeated engine map/sensor teardown.
        if world.get_map().name.rsplit('/', 1)[-1] != 'Town01':
            client.set_timeout(120.0)
            world = client.load_world('Town01')
        client.set_timeout(10.0)
        original = world.get_settings()
        settings = world.get_settings()
        settings.synchronous_mode = True
        settings.fixed_delta_seconds = dt
        world.apply_settings(settings)
        road_map = world.get_map()
        library = world.get_blueprint_library()
        blueprint = library.find('vehicle.tesla.model3')
        ego = lead = None
        # Check the entire route, not just its endpoint: exclude bends and junctions.
        for spawn in road_map.get_spawn_points():
            wp = road_map.get_waypoint(spawn.location)
            forward = wp.transform.get_forward_vector()
            length = args.gap + 20 + (profile.sample(profile.end_time)[0] if profile else 0)
            samples = [wp]
            for distance in range(2, math.ceil(length) + 1, 2):
                candidates = wp.next(float(distance))
                if len(candidates) != 1:
                    break
                point = candidates[0]
                delta = point.transform.location - wp.transform.location
                yaw_error = (point.transform.rotation.yaw - wp.transform.rotation.yaw + 180) % 360 - 180
                if (point.is_junction or point.road_id != wp.road_id or point.lane_id != wp.lane_id
                        or abs(yaw_error) > 0.5 or abs(-forward.y * delta.x + forward.x * delta.y) > 0.1):
                    break
                samples.append(point)
            if wp.is_junction or len(samples) != len(range(2, math.ceil(length) + 1, 2)) + 1:
                continue
            ego = world.try_spawn_actor(blueprint, spawn)
            if ego is None:
                continue
            actors.append(ego)
            center_distance = args.gap + 2 * ego.bounding_box.extent.x
            lead_wp = wp.next(center_distance)[0]
            lead_transform = lead_wp.transform
            lead_transform.location.z += spawn.location.z - wp.transform.location.z
            lead = world.try_spawn_actor(blueprint, lead_transform)
            if lead is None:
                if not ego.destroy():
                    raise RuntimeError('Failed to clean unsuccessful spawn')
                actors.remove(ego)
                ego = None
                continue
            actors.append(lead)
            break
        if ego is None or lead is None:
            raise RuntimeError('No unoccupied straight lane long enough for the requested gap')
        steering_path = [(p.transform.location.x, -p.transform.location.y, p.transform.location.z) for p in samples]
        axis = (forward.x, forward.y, forward.z)
        collision = world.spawn_actor(library.find('sensor.other.collision'), carla.Transform(), attach_to=ego)
        actors.append(collision)
        collision.listen(lambda event: collisions.append(dict(frame=event.frame, other=event.other_actor.type_id)))
        for vehicle in (ego, lead):
            vehicle.apply_control(carla.VehicleControl(brake=1.0))
        for _ in range(20):
            world.tick(20.0)
        lead.set_simulate_physics(False)
        lead_origin = lead.get_location()
        origin = ego.get_location()
        cruise = args.speed_kmh / 3.6
        ego.apply_control(carla.VehicleControl())
        # Spin up wheel dynamics before the measured experiment. Velocity is
        # never forced inside the subsequent distance-control loop.
        for _ in range(20):
            ego.set_target_velocity(carla.Vector3D(*(v * cruise for v in axis)))
            world.tick(20.0)
        et, lt = ego.get_transform(), lead.get_transform()
        ego_vertices = [(v.x, v.y, v.z) for v in ego.bounding_box.get_world_vertices(et)]
        lead_vertices = [(v.x, v.y, v.z) for v in lead.bounding_box.get_world_vertices(lt)]
        # One camera warm-up tick follows, so allow for that forward travel.
        correction = args.gap + cruise * dt - projected_gap(ego_vertices, lead_vertices, axis)
        lt.location += carla.Location(*(v * correction for v in axis))
        lead.set_transform(lt)
        lead_origin = lt.location
        origin = ego.get_location()
        camera_bp = library.find('sensor.camera.rgb')
        camera_bp.set_attribute('image_size_x', '960')
        camera_bp.set_attribute('image_size_y', '540')
        camera_transform = carla.Transform(carla.Location(x=-7, z=4), carla.Rotation(pitch=-18))
        camera = world.spawn_actor(camera_bp, camera_transform, attach_to=ego)
        actors.append(camera)
        images = queue.Queue()
        camera.listen(images.put)
        world.tick(20.0)

        def measure(snapshot):
            ego_state, lead_state = snapshot.find(ego.id), snapshot.find(lead.id)
            if ego_state is None or lead_state is None:
                raise RuntimeError('Vehicle absent from current snapshot')
            et, lt = ego_state.get_transform(), lead_state.get_transform()
            def vertices(box, transform):
                return [(v.x, v.y, v.z) for v in box.get_world_vertices(transform)]
            gap = projected_gap(vertices(ego.bounding_box, et), vertices(lead.bounding_box, lt), axis)
            geometry = reference_to_body_offset(vertices(ego.bounding_box, et), vertices(lead.bounding_box, lt),
                (et.location.x, et.location.y, et.location.z), (lt.location.x, lt.location.y, lt.location.z), axis)
            offset = et.location - origin
            lateral = -forward.y * offset.x + forward.x * offset.y
            lane_offset = et.location - wp.transform.location
            lane_lateral = -forward.y * lane_offset.x + forward.x * lane_offset.y
            yaw_error = (et.rotation.yaw - spawn.rotation.yaw + 180) % 360 - 180
            return dict(frame=snapshot.frame, time=snapshot.timestamp.elapsed_seconds,
                        gap_m=gap, speed_mps=ego_state.get_velocity().length(),
                        longitudinal_origin_distance_m=sum(v * a for v, a in zip(
                            (lt.location.x-et.location.x, lt.location.y-et.location.y, lt.location.z-et.location.z), axis)),
                        center_distance_m=et.location.distance(lt.location), lateral_m=lateral,
                        measured_reference_offset_m=geometry,
                        lane_center_error_m=lane_lateral,
                        yaw_error_deg=yaw_error, lead_displacement_m=lt.location.distance(lead_origin),
                        x=et.location.x, y=et.location.y, yaw_deg=et.rotation.yaw)

        def save_image(frame, name):
            while True:
                picture = images.get(timeout=20.0)
                if picture.frame == frame:
                    picture.save_to_disk(str(args.output / name))
                    return
                if picture.frame > frame:
                    raise RuntimeError('Camera and state frames are inconsistent')

        snapshot = world.get_snapshot()
        first = measure(snapshot)
        first['elapsed'] = 0.0

        def validate_state(state):
            if state['gap_m'] <= 0 or collisions:
                raise RuntimeError('Collision or body overlap detected')
            if abs(state['lateral_m']) > 0.4 or abs(state['yaw_error_deg']) > 3:
                raise RuntimeError('Vehicle left the supported straight-line corridor')
            distance = profile.sample(state['time'] - first['time'])[0] if profile else 0
            if abs(state['lead_displacement_m'] - distance) > 0.05:
                raise RuntimeError('Lead vehicle differs from prescribed position')
        report.update(map=road_map.name, vehicle=ego.type_id, initial=first,
                      ego_box_extent=dict(x=ego.bounding_box.extent.x, y=ego.bounding_box.extent.y),
                      road_id=wp.road_id, lane_id=wp.lane_id, fixed_delta_seconds=dt,
                      formal_path=dict(start=steering_path[0], end=steering_path[-1], points=len(steering_path),
                                       coordinate_system='x,-CARLA_y,z',
                                       sha256=hashlib.sha256(json.dumps(steering_path).encode()).hexdigest()))
        if abs(first['gap_m'] - args.gap) > 0.5 or abs(first['speed_mps'] - cruise) > 0.5:
            raise RuntimeError('Initial gap or speed outside experiment tolerance')
        print(f"Initial: {first['speed_mps'] * 3.6:.2f} km/h, body gap {first['gap_m']:.2f} m", flush=True)
        braking_saved = False
        stopped_saved = False
        stop = None
        for step in range(round(args.timeout / dt)):
            started = time.monotonic()
            state = measure(snapshot)
            state['elapsed'] = state['time'] - first['time']
            lead_distance, lead_speed, lead_phase = profile.sample(state['elapsed']) if profile else (0, 0, 'stationary')
            state.update(lead_speed_mps=lead_speed, lead_phase=lead_phase,
                         lead_position_error_m=abs(state['lead_displacement_m'] - lead_distance))
            validate_state(state)
            formal = None
            geometry_offset = state['measured_reference_offset_m'] if args.distance_geometry == 'body' else None
            if following is not None:
                formal = following.step(0, state['speed_mps'],
                        [(state['longitudinal_origin_distance_m'], lead_speed, 0 if lead_speed < 0.01 else 2, 1, 1)],
                        case_index=report['case_index'], static_reference_offset=geometry_offset, policy=policy)
                target = formal['target_speed']
                throttle, brake = speed_actuators(target, state['speed_mps'])
            else:
                target, throttle, brake, _ = stop_command(state['gap_m'], state['speed_mps'], cruise, args.stop_gap)
            # The front car remains present: ready means dwell completed, not permission to drive into it.
            stop = shared.step(state['time'], state['speed_mps'],
                               -0.5 <= state['gap_m'] - args.stop_gap <= 0.15,
                               target, release_allowed=False)
            if following is None:
                target = stop.target_speed
                if stop.phase != 'approaching':
                    throttle, brake = 0.0, 1.0
            if stop.phase == 'invalid':
                raise RuntimeError('Shared stop controller rejected discontinuous or invalid frame input')
            lateral = steering.step(steering_path, *carla_to_formal(state['x'], state['y'], state['yaw_deg']),
                                    state['speed_mps']) if steering else None
            applied_steer = max(-1.0, min(1.0, lateral['command'])) if lateral else 0.0
            ego.apply_control(carla.VehicleControl(throttle=throttle, brake=brake, steer=applied_steer))
            if profile:
                distance = profile.sample(state['elapsed'] + dt)[0]
                lead.set_transform(carla.Transform(
                    lead_origin + carla.Location(*(v * distance for v in axis)), lt.rotation))
            world.get_spectator().set_transform(camera.get_transform())
            frame = world.tick(20.0)
            after = world.get_snapshot()
            result = measure(after)
            rows.append(dict(**state, target_speed_mps=target, throttle=throttle, brake=brake,
                             static_reference_offset_m=geometry_offset if following is not None and geometry_offset is not None else '',
                             steering=applied_steer, formal_steering=lateral['command'] if lateral else '',
                             pursuit_raw=lateral['raw'] if lateral else '',
                             pursuit_target_index=lateral['target_index'] if lateral else '',
                             formal_target_speed_mps=formal['target_speed'] if formal else '',
                             formal_selected_s=formal['selected_s'] if formal else '',
                             stop_phase=stop.phase, controller_held_seconds=stop.held_seconds,
                             dwell_ready=stop.ready, result_time=result['time'],
                             result_frame=frame, result_gap_m=result['gap_m'], result_speed_mps=result['speed_mps']))
            validate_state(result)
            name = 'start.png' if step == 0 else None
            if brake > 0.05 and not braking_saved:
                name, braking_saved = 'braking.png', True
            if stop.ready and target == 0 and result['speed_mps'] <= 0.1 and not stopped_saved:
                name, stopped_saved = 'stopped.png', True
            elif step == round(args.timeout / dt) - 1:
                name = 'final.png'
            if profile and result['time'] - first['time'] >= profile.end_time:
                name = 'final.png'
            if name:
                save_image(frame, name)
            else:
                while not images.empty():
                    images.get_nowait()
            if step % 20 == 0:
                print(f"t={step * dt:5.1f}s gap={result['gap_m']:6.2f}m speed={result['speed_mps'] * 3.6:5.2f}km/h phase={stop.phase} held={stop.held_seconds:.2f}s", flush=True)
            snapshot = after
            if not args.no_realtime:
                time.sleep(max(0.0, dt - (time.monotonic() - started)))
            if not profile and stop.ready and target == 0 and result['speed_mps'] <= 0.1:
                break
            if profile and result['time'] - first['time'] >= profile.end_time:
                break
        final = measure(snapshot)
        final['elapsed'] = final['time'] - first['time']
        observed = observed_dwell(rows + [final], args.stop_gap)
        duration_ok = observed > args.dwell_seconds if args.exclusive_dwell else observed >= args.dwell_seconds
        report.update(final=final, frames=len(rows), stop_held_seconds=observed,
                      stationary_seconds=observed_dwell(rows + [final], None),
                      target_restart_count=sum(a['target_speed_mps'] == 0 and b['target_speed_mps'] > 0
                                               for a, b in zip(rows, rows[1:])),
                      controller_held_seconds=stop.held_seconds if stop else 0,
                      elapsed_sim_seconds=final['time'] - first['time'],
                      min_gap_m=min([first['gap_m']] + [row['result_gap_m'] for row in rows]),
                      max_lateral_m=max(abs(row['lateral_m']) for row in rows + [final]),
                      max_lane_center_error_m=max(abs(row['lane_center_error_m']) for row in rows + [final]),
                      stop_error_m=final['gap_m'] - args.stop_gap,
                      passed=(stop is not None and stop.ready and duration_ok and final['speed_mps'] <= 0.1
                              and (following is None or rows[-1]['formal_target_speed_mps'] == 0)
                              and abs(final['gap_m'] - args.stop_gap) <= 0.5
                              and final['gap_m'] > 0 and not collisions))
        if args.scenario == 'lead-brake':
            # Last measured state follows the last sent command, held through this tick.
            final['formal_target_speed_mps'] = rows[-1]['formal_target_speed_mps']
            assessment = assess_braking(rows + [final], profile, args.gap, args.stop_gap,
                                         args.dwell_seconds, args.exclusive_dwell)
            report.update(braking_assessment=assessment, passed=assessment['passed'] and not collisions)
            if not report['passed']:
                report['error'] = assessment['error']
        elif profile:
            assessment = assess_stop_go(rows + [final], profile)
            report.update(stop_go_assessment=assessment, passed=assessment['passed'] and not collisions)
            if not report['passed']:
                report['error'] = assessment['error']
        elif not report['passed']:
            reasons = []
            if abs(final['gap_m'] - args.stop_gap) > 0.5:
                reasons.append('Final gap outside +/-0.5 m experimental tolerance')
            if not duration_ok:
                reasons.append('Required continuous standstill within target region not observed')
            if following is not None and rows[-1]['formal_target_speed_mps'] != 0:
                reasons.append('Production code still requests motion')
            report['error'] = '; '.join(reasons) or 'Stop observation or final speed acceptance failed'
    except (Exception, KeyboardInterrupt) as exc:
        report['passed'] = False
        report['error'] = f'{type(exc).__name__}: {exc}'
    finally:
        if steering is not None:
            steering.close()
        if following is not None:
            following.close()
        if shared is not None:
            shared.close()
        client.set_timeout(5.0)
        cleanup_errors = []
        for actor in reversed(actors):
            try:
                if actor.type_id.startswith('sensor.'):
                    actor.stop()
                if not actor.destroy():
                    cleanup_errors.append(f'destroy returned false: {actor.id}')
            except Exception as exc:
                cleanup_errors.append(str(exc))
        if world is not None and original is not None:
            try:
                world.apply_settings(original)
            except Exception as exc:
                cleanup_errors.append(str(exc))
        report.update(collisions=collisions, cleanup_errors=cleanup_errors,
                      passed=report['passed'] and not cleanup_errors and not collisions)
        if rows:
            with (args.output / 'frames.csv').open('w', newline='', encoding='utf-8') as out:
                writer = csv.DictWriter(out, fieldnames=list(rows[0]))
                writer.writeheader()
                writer.writerows(rows)
        (args.output / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    if args.scenario == 'stationary' and following is not None and 'final' in report:
        # Post-run diagnosis only: cannot change controls or the original verdict.
        try:
            if __package__:
                from .analyze_stationary_stop import save_analysis
            else:
                from analyze_stationary_stop import save_analysis
            save_analysis(args.output)
            print(f'Stop diagnosis: {args.output / "stop-analysis.json"}')
        except Exception as exc:
            # Diagnostics must not turn a completed physical run into an error.
            print(f'Stop diagnosis unavailable: {exc}')
    print(json.dumps(report, indent=2))
    print(f'Results: {args.output.resolve()}')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(run(parse_args()))
