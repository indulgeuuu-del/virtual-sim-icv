"""Fixed virtual stop-line baseline using production C++ selection/braking.

Requires an exclusive CARLA server. No signal detection, junction logic or
release phase. Rules reference: 2027 scoring PDF physical page 19, table 22.
"""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import subprocess
import time

if __package__:
    from .competition_stopline import StopLineController
    from .competition_steering import SteeringController, carla_to_formal
    from .carla_stationary_stop import PISpeedActuators, prepare_ego_transform
else:
    from competition_stopline import StopLineController
    from competition_steering import SteeringController, carla_to_formal
    from carla_stationary_stop import PISpeedActuators, prepare_ego_transform


def assess(rows, collisions):
    if not rows:
        return dict(passed=False, reason='no samples')
    if (any(not all(math.isfinite(r[k]) for k in ('time','speed_mps','front_gap_m','target_mps')) for r in rows)
        or any(not 0 < b['time']-a['time'] <= 0.25 for a,b in zip(rows,rows[1:]))):
        return dict(passed=False, reason='invalid or interrupted samples')
    held = 0.0
    for a, b in zip(reversed(rows[:-1]), reversed(rows[1:])):
        if a['speed_mps'] > 0.1 or b['speed_mps'] > 0.1 or not 0 < b['time']-a['time'] <= 0.25:
            break
        held += b['time']-a['time']
    gap = rows[-1]['front_gap_m']
    crossed = any(r['front_gap_m'] < 0 for r in rows)
    passed = not collisions and not crossed and 0 <= gap <= 0.5 and held > 5 and rows[-1]['target_mps'] == 0
    return dict(passed=passed, final_front_gap_m=gap, held_seconds=held, crossed=crossed,
        distance_in_half_meter=0 <= gap <= 0.5,
        reason='fixed-line engineering subset only; excludes signal recognition, release and official scoring')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--host', default='localhost');p.add_argument('--port', type=int, default=2000)
    p.add_argument('--speed-kmh', type=float, default=30)
    p.add_argument('--distance', type=float, default=60, help='Initial vehicle reference point to fixed line, m')
    p.add_argument('--fixed-delta-seconds', type=float, default=0.05)
    p.add_argument('--seconds', type=float, default=45)
    p.add_argument('--no-realtime', action='store_true', help='Run as fast as the server allows')
    p.add_argument('--no-follow-camera', action='store_true', help='Leave the spectator camera under manual control')
    p.add_argument('--output', type=Path, default=Path('work/carla/stopline-baseline'))
    a = p.parse_args()
    if not all(math.isfinite(v) for v in (a.speed_kmh,a.distance,a.fixed_delta_seconds,a.seconds)) or not (0<a.speed_kmh<=50 and a.distance>10 and 0<a.fixed_delta_seconds<=0.1 and a.seconds>5):
        p.error('Invalid experiment parameters')
    import carla
    a.output.mkdir(parents=True, exist_ok=True)
    if (a.output/'report.json').exists():
        p.error('Output already contains a report; choose a new --output')
    controller=StopLineController();steering=SteeringController();pedals=PISpeedActuators()
    client=carla.Client(a.host,a.port);client.set_timeout(30)
    world=None;original=None;spectator=None;original_view=None;actors=[];rows=[];collisions=[];cleanup=[]
    report=dict(passed=False, sdk_verified=False, official_score_verified=False,
        source_build=controller.build, steering_build=steering.build,
        git_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=Path(__file__).resolve().parents[1],text=True).strip(),
        parameter_sha256=controller.parameter_sha256, parameters={k:str(v) if isinstance(v,Path) else v for k,v in vars(a).items()},
        script_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        scope='actual nearest-line and Whole braking bodies; CARLA pedals/geometry; no complete drive or main_track',
        rule='2027 scoring physical p19 table22: >0.5m and <=1m penalty; >1m or hold <=5s zero; this is only a subset')
    try:
        world=client.get_world()
        if any(x.type_id.startswith(('vehicle.','sensor.','walker.')) for x in world.get_actors()):
            raise RuntimeError('CARLA server occupied')
        if world.get_map().name.rsplit('/',1)[-1]!='Town01':
            client.set_timeout(120);world=client.load_world('Town01');client.set_timeout(30)
        original=world.get_settings();settings=world.get_settings()
        settings.synchronous_mode=True;settings.fixed_delta_seconds=a.fixed_delta_seconds
        world.apply_settings(settings)
        road=world.get_map();bp=world.get_blueprint_library().find('vehicle.tesla.model3')
        ego=None
        for index,spawn in enumerate(road.get_spawn_points()):
            wp=road.get_waypoint(spawn.location);forward=wp.transform.get_forward_vector()
            if wp.is_junction: continue
            path=[wp]
            for d in range(2,math.ceil(a.distance+20)+1,2):
                candidates=wp.next(d)
                if len(candidates)!=1: break
                q=candidates[0];delta=q.transform.location-wp.transform.location
                yaw=(q.transform.rotation.yaw-wp.transform.rotation.yaw+180)%360-180
                if q.is_junction or (q.road_id,q.lane_id)!=(wp.road_id,wp.lane_id) or abs(yaw)>0.5 or abs(-forward.y*delta.x+forward.x*delta.y)>0.1: break
                path.append(q)
            if len(path)!=len(range(2,math.ceil(a.distance+20)+1,2))+1: continue
            ego=world.try_spawn_actor(bp,prepare_ego_transform(carla,spawn,wp.transform,'lane-center'))
            if ego is not None: break
        if ego is None: raise RuntimeError('No free straight route')
        actors.append(ego)
        sensor=world.spawn_actor(world.get_blueprint_library().find('sensor.other.collision'),carla.Transform(),attach_to=ego)
        actors.append(sensor);sensor.listen(lambda e:collisions.append(dict(frame=e.frame,other=e.other_actor.type_id)))
        ego.apply_control(carla.VehicleControl(brake=1,manual_gear_shift=True,gear=1))
        for _ in range(20): world.tick(20)
        origin=ego.get_location();axis=(forward.x,forward.y)
        if not a.no_follow_camera:
            spectator=world.get_spectator();original_view=spectator.get_transform()

        def follow_camera():
            if spectator is not None:
                pose=ego.get_transform();direction=pose.get_forward_vector()
                spectator.set_transform(carla.Transform(
                    carla.Location(pose.location.x-10*direction.x,pose.location.y-10*direction.y,pose.location.z+6),
                    carla.Rotation(pitch=-25,yaw=pose.rotation.yaw)))

        follow_camera()
        line=(origin.x+axis[0]*a.distance,origin.y+axis[1]*a.distance)
        formal_path=[(q.transform.location.x,-q.transform.location.y,q.transform.location.z) for q in path]
        report.update(platform=client.get_server_version(),map=road.name,vehicle=ego.type_id,
            spawn_index=index,road_id=wp.road_id,lane_id=wp.lane_id,line_xy=line,
            legacy_front_offset_m=4.38046,margin_m=controller.margin)
        world.debug.draw_line(carla.Location(line[0]-axis[1]*2,line[1]+axis[0]*2,origin.z),
            carla.Location(line[0]+axis[1]*2,line[1]-axis[0]*2,origin.z),thickness=0.1,color=carla.Color(255,0,0),life_time=a.seconds)
        print(f'CARLA: {a.seconds:g}s simulation, '
              f'{"fast" if a.no_realtime else "real-time"}, '
              f'{"manual camera" if a.no_follow_camera else "following camera"}. '
              'Actors and camera will be restored at the end. Ctrl+C stops the experiment.',flush=True)
        if not a.no_realtime:
            print('Starting in 3 seconds; switch to the CARLA window.',flush=True)
            time.sleep(3)
        playback_started=time.monotonic()
        for _ in range(math.ceil(a.seconds/a.fixed_delta_seconds)):
            frame_started=time.monotonic()
            snapshot=world.get_snapshot();pose=ego.get_transform();location=pose.location
            speed=ego.get_velocity().length()
            reference=(line[0]-location.x)*axis[0]+(line[1]-location.y)*axis[1]
            vertices=ego.bounding_box.get_world_vertices(pose)
            front=max((v.x-location.x)*axis[0]+(v.y-location.y)*axis[1] for v in vertices)
            lateral=-(location.x-origin.x)*axis[1]+(location.y-origin.y)*axis[0]
            yaw=(pose.rotation.yaw-wp.transform.rotation.yaw+180)%360-180
            if abs(lateral)>0.4 or abs(yaw)>3: raise RuntimeError('Left straight experiment corridor')
            target=controller.step(reference,speed,a.speed_kmh/3.6)
            throttle,brake=pedals.step(target,speed,a.fixed_delta_seconds)
            steer=steering.step(formal_path,*carla_to_formal(location.x,location.y,pose.rotation.yaw),speed)['command']
            rows.append(dict(frame=snapshot.frame,time=snapshot.timestamp.elapsed_seconds,speed_mps=speed,
                reference_distance_m=reference,front_offset_m=front,front_gap_m=reference-front,target_mps=target,
                throttle=throttle,brake=brake,steering=steer,lateral_m=lateral))
            ego.apply_control(carla.VehicleControl(throttle=throttle,brake=brake,steer=max(-1,min(1,steer)),manual_gear_shift=True,gear=1))
            world.tick(20)
            follow_camera()
            if not a.no_realtime:
                time.sleep(max(0.0,a.fixed_delta_seconds-(time.monotonic()-frame_started)))
        report['playback_wall_seconds']=time.monotonic()-playback_started
        report.update(assessment=assess(rows,collisions))
        report['passed']=report['assessment']['passed']
    except KeyboardInterrupt:
        report['error']='Interrupted by user'
    except Exception as e:
        report['error']=f'{type(e).__name__}: {e}'
    finally:
        if spectator is not None and original_view is not None:
            try: spectator.set_transform(original_view)
            except Exception as e: cleanup.append(f'camera restore: {e}')
        for actor in reversed(actors):
            try:
                if actor.type_id.startswith('sensor.'): actor.stop()
                if not actor.destroy(): cleanup.append(f'destroy failed {actor.id}')
            except Exception as e: cleanup.append(str(e))
        if world is not None and original is not None:
            try: world.apply_settings(original)
            except Exception as e: cleanup.append(str(e))
        steering.close()
        report.update(collisions=collisions,cleanup_errors=cleanup,samples=len(rows))
        if cleanup or collisions: report['passed']=False
        if rows:
            with (a.output/'frames.csv').open('w',newline='',encoding='utf-8') as f:
                writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
        (a.output/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report.get('assessment',report.get('error')),indent=2))
    return 0 if report['passed'] else 1


if __name__=='__main__':
    raise SystemExit(main())
