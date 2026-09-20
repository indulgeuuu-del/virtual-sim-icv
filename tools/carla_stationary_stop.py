"""Stop behind a stationary vehicle on a verified straight Town01 lane.

Exclusive local CARLA server required: loads Town01 if necessary.
Uses ground-truth vehicle boxes, not competition sensor input or legacy code.
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


def projected_gap(ego_points, lead_points, axis):
    """Signed separation of body boxes along a fixed unit road direction."""
    def project(point):
        return sum(a * b for a, b in zip(point, axis))
    return min(map(project, lead_points)) - max(map(project, ego_points))


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
    parser.add_argument('--speed-kmh', type=float, default=10.0)
    parser.add_argument('--gap', type=float, default=25.0, help='Initial body gap in m')
    parser.add_argument('--stop-gap', type=float, default=2.0, help='Desired final body gap in m')
    parser.add_argument('--timeout', type=float, default=60.0, help='Simulation seconds')
    parser.add_argument('--no-realtime', action='store_true', help='Run without wall-clock pacing')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if not all(math.isfinite(v) for v in (args.speed_kmh, args.gap, args.stop_gap, args.timeout)):
        parser.error('Numeric arguments must be finite')
    if not 1 <= args.speed_kmh <= 30 or not 1 <= args.stop_gap <= 5:
        parser.error('Supported speed: 1..30 km/h; stop gap: 1..5 m')
    if not args.stop_gap + 5 <= args.gap <= 65 or not 5 <= args.timeout <= 120:
        parser.error('Initial gap: stop-gap+5..65 m; timeout: 5..120 s')
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
    report = dict(passed=False, competition_verified=False, input_source='CARLA ground truth',
                  parameters={k: str(v) if isinstance(v, Path) else v for k, v in vars(args).items()},
                  source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    client = carla.Client('127.0.0.1', 2000)
    client.set_timeout(10.0)
    try:
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
        settings.fixed_delta_seconds = 0.05
        world.apply_settings(settings)
        road_map = world.get_map()
        library = world.get_blueprint_library()
        blueprint = library.find('vehicle.tesla.model3')
        ego = lead = None
        # Check the entire route, not just its endpoint: exclude bends and junctions.
        for spawn in road_map.get_spawn_points():
            wp = road_map.get_waypoint(spawn.location)
            forward = wp.transform.get_forward_vector()
            length = args.gap + 20
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
        correction = args.gap + cruise * 0.05 - projected_gap(ego_vertices, lead_vertices, axis)
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
            offset = et.location - origin
            lateral = -forward.y * offset.x + forward.x * offset.y
            yaw_error = (et.rotation.yaw - spawn.rotation.yaw + 180) % 360 - 180
            return dict(frame=snapshot.frame, time=snapshot.timestamp.elapsed_seconds,
                        gap_m=gap, speed_mps=ego_state.get_velocity().length(),
                        center_distance_m=et.location.distance(lt.location), lateral_m=lateral,
                        yaw_error_deg=yaw_error, lead_displacement_m=lt.location.distance(lead_origin),
                        x=et.location.x, y=et.location.y)

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
        report.update(map=road_map.name, vehicle=ego.type_id, initial=first,
                      ego_box_extent=dict(x=ego.bounding_box.extent.x, y=ego.bounding_box.extent.y),
                      road_id=wp.road_id, lane_id=wp.lane_id, fixed_delta_seconds=0.05)
        if abs(first['gap_m'] - args.gap) > 0.5 or abs(first['speed_mps'] - cruise) > 0.5:
            raise RuntimeError('Initial gap or speed outside experiment tolerance')
        print(f"Initial: {first['speed_mps'] * 3.6:.2f} km/h, body gap {first['gap_m']:.2f} m", flush=True)
        held = 0
        latched = braking_saved = False
        for step in range(round(args.timeout / 0.05)):
            started = time.monotonic()
            state = measure(snapshot)
            if state['gap_m'] <= 0 or collisions:
                raise RuntimeError('Collision or body overlap detected')
            if abs(state['lateral_m']) > 0.4 or abs(state['yaw_error_deg']) > 3:
                raise RuntimeError('Vehicle left the supported straight-line corridor')
            if state['lead_displacement_m'] > 0.05:
                raise RuntimeError('Lead vehicle moved')
            target, throttle, brake, hold = stop_command(state['gap_m'], state['speed_mps'], cruise, args.stop_gap)
            latched = latched or hold
            if latched:
                target, throttle, brake = 0.0, 0.0, 1.0
            ego.apply_control(carla.VehicleControl(throttle=throttle, brake=brake))
            world.get_spectator().set_transform(camera.get_transform())
            frame = world.tick(20.0)
            after = world.get_snapshot()
            result = measure(after)
            rows.append(dict(**state, target_speed_mps=target, throttle=throttle, brake=brake,
                             result_frame=frame, result_gap_m=result['gap_m'], result_speed_mps=result['speed_mps']))
            held = held + 1 if latched and result['speed_mps'] < 0.1 else 0
            name = 'start.png' if step == 0 else None
            if brake > 0.05 and not braking_saved:
                name, braking_saved = 'braking.png', True
            if held >= 40:
                name = 'stopped.png'
            if name:
                save_image(frame, name)
            else:
                while not images.empty():
                    images.get_nowait()
            if step % 20 == 0:
                print(f"t={step * 0.05:5.1f}s gap={result['gap_m']:6.2f}m speed={result['speed_mps'] * 3.6:5.2f}km/h target={target * 3.6:5.2f}", flush=True)
            snapshot = after
            if not args.no_realtime:
                time.sleep(max(0.0, 0.05 - (time.monotonic() - started)))
            if held >= 40:
                break
        final = measure(snapshot)
        report.update(final=final, frames=len(rows), stop_held_seconds=held * 0.05,
                      elapsed_sim_seconds=final['time'] - first['time'],
                      min_gap_m=min([first['gap_m']] + [row['result_gap_m'] for row in rows]),
                      max_lateral_m=max(abs(row['lateral_m']) for row in rows),
                      stop_error_m=final['gap_m'] - args.stop_gap,
                      passed=(held >= 40 and abs(final['gap_m'] - args.stop_gap) <= 0.5
                              and final['gap_m'] > 0 and not collisions))
        if not report['passed']:
            report['error'] = 'Timeout or final gap outside +/-0.5 m acceptance tolerance'
    except (Exception, KeyboardInterrupt) as exc:
        report['passed'] = False
        report['error'] = f'{type(exc).__name__}: {exc}'
    finally:
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
    print(json.dumps(report, indent=2))
    print(f'Results: {args.output.resolve()}')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(run(parse_args()))
