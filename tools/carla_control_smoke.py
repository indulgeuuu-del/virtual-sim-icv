"""Exercise CARLA throttle/brake on an empty Town01 straight lane.

Uses simulator state, not perception or the inherited competition controller.
Requires exclusive use of the local server; loading Town01 resets its world.
"""

import argparse
import json
import math
import queue
from pathlib import Path

import carla


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('work/carla/control_smoke'))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    client = carla.Client('127.0.0.1', 2000)
    client.set_timeout(120.0)
    actors, rows, collisions = [], [], []
    world = None
    original = None
    report = dict(passed=False, competition_verified=False,
                  experiment='single vehicle throttle and brake; no obstacle or lane controller')
    try:
        world = client.load_world('Town01')
        original = world.get_settings()
        settings = world.get_settings()
        settings.synchronous_mode = True
        settings.fixed_delta_seconds = 0.05
        world.apply_settings(settings)
        road_map = world.get_map()
        library = world.get_blueprint_library()
        vehicle = None
        for spawn in road_map.get_spawn_points():
            waypoint = road_map.get_waypoint(spawn.location)
            ahead = waypoint.next(35.0)
            if waypoint.is_junction or not ahead or ahead[0].is_junction:
                continue
            delta = (ahead[0].transform.rotation.yaw - waypoint.transform.rotation.yaw + 180) % 360 - 180
            if abs(delta) > 1 or ahead[0].road_id != waypoint.road_id:
                continue
            vehicle = world.try_spawn_actor(library.find('vehicle.tesla.model3'), spawn)
            if vehicle is not None:
                break
        if vehicle is None:
            raise RuntimeError('No free straight spawn with 35 m available')
        actors.append(vehicle)
        collision = world.spawn_actor(library.find('sensor.other.collision'), carla.Transform(), attach_to=vehicle)
        actors.append(collision)
        collision.listen(lambda event: collisions.append(event.frame))
        camera_bp = library.find('sensor.camera.rgb')
        camera_bp.set_attribute('image_size_x', '800')
        camera_bp.set_attribute('image_size_y', '450')
        camera = world.spawn_actor(camera_bp, carla.Transform(carla.Location(x=-6, z=3), carla.Rotation(pitch=-15)), attach_to=vehicle)
        actors.append(camera)
        images = queue.Queue()
        camera.listen(images.put)
        initial = None
        saved_image = False
        for step in range(180):
            # Settle for 1 s, accelerate for 3 s, then brake for 5 s.
            accelerating = 20 <= step < 80
            control = carla.VehicleControl(throttle=0.25 if accelerating else 0.0,
                                           brake=0.0 if accelerating else 1.0)
            vehicle.apply_control(control)
            frame = world.tick(20.0)
            snapshot = world.get_snapshot()
            state = snapshot.find(vehicle.id)
            location = state.get_transform().location
            speed = state.get_velocity().length()
            if step == 20:
                initial = location
            rows.append(dict(frame=frame, time=snapshot.timestamp.elapsed_seconds,
                             x=location.x, y=location.y, z=location.z, speed_mps=speed,
                             throttle=control.throttle, brake=control.brake))
            if step == 79:
                while True:
                    image = images.get(timeout=20.0)
                    if image.frame == frame:
                        image.save_to_disk(str(args.output / 'driving.png'))
                        saved_image = True
                        break
                    if image.frame > frame:
                        raise RuntimeError('Camera frame advanced beyond requested frame')
            else:
                while not images.empty():
                    images.get_nowait()
        peak = max(row['speed_mps'] for row in rows)
        final = rows[-1]['speed_mps']
        travel = math.hypot(rows[-1]['x'] - initial.x, rows[-1]['y'] - initial.y)
        report.update(map=road_map.name, vehicle=vehicle.type_id,
                      client_version=client.get_client_version(), server_version=client.get_server_version(),
                      frames=len(rows), peak_speed_mps=peak, final_speed_mps=final,
                      displacement_m=travel, collision_frames=collisions,
                      image_saved=saved_image,
                      passed=peak > 1 and final < 0.1 and travel > 1 and not collisions and saved_image)
    except Exception as exc:
        report['error'] = f'{type(exc).__name__}: {exc}'
    finally:
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
        report['cleanup_errors'] = cleanup_errors
        report['passed'] = report['passed'] and not cleanup_errors
        (args.output / 'frames.json').write_text(json.dumps(rows, indent=2), encoding='utf-8')
        (args.output / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
