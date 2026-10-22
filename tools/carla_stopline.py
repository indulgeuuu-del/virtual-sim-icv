"""Stop-line and map-signal experiments using shared production C++ control.

Requires an exclusive CARLA server. Fixed-line or actual map-lamp experiment;
no visual recognition/full junction logic. Rules: scoring PDF p19 table22, p15 table12.
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
    from .competition_stopline import StopLineController, StopLineSequence, SignalStopTask
    from .competition_steering import SteeringController, carla_to_formal
    from .carla_stationary_stop import PISpeedActuators, prepare_ego_transform
    from .carla_signal import approaches, observe, assess_signal, assess_transitions, assess_passage
else:
    from competition_stopline import StopLineController, StopLineSequence, SignalStopTask
    from competition_steering import SteeringController, carla_to_formal
    from carla_stationary_stop import PISpeedActuators, prepare_ego_transform
    from carla_signal import approaches, observe, assess_signal, assess_transitions, assess_passage


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


def assess_release(rows,collisions):
    """Independent measurements, not controller ready/held flags.

    p19 stop subset plus p15's <=5s departure response as a separate engineering
    check; combining them does not imply both belong to one official scenario.
    """
    if (not rows or any(not all(math.isfinite(r[k]) for k in
        ('time','speed_mps','front_gap_m','target_mps')) for r in rows)
        or any(not 0<b['time']-a['time']<=.25 for a,b in zip(rows,rows[1:]))):
        return dict(passed=False,reason='invalid or interrupted release samples')
    first=next((i for i,r in enumerate(rows) if r['release_allowed']),None)
    if first is None or first==0:
        return dict(passed=False,reason='no observed stop before release')
    stopped=assess(rows[:first],collisions)
    after=rows[first:]
    if any(not r['release_allowed'] for r in after):
        return dict(passed=False,reason='release permission discontinuous')
    moved=next((r for r in after if r['speed_mps']>.1),None)
    delay=None if moved is None else moved['time']-after[0]['time']
    cleared=after[-1]['front_gap_m']<=-5
    return dict(passed=stopped['passed'] and delay is not None and delay<=5 and cleared and not collisions,
        stop_assessment=stopped,departure_delay_s=delay,cleared_line=cleared,
        reason='synthetic permission; p19 stop subset and separate p15 response check; no signal recognition or official score')


def assess_signal_tasks(rows,collisions):
    # Segments must be contiguous, complete and ordered; do not accept one success
    # twice or conceal samples dropped at a task switch.
    if (not rows or any(not 0<b['time']-a['time']<=.25 for a,b in zip(rows,rows[1:]))
        or any(r['task_id'] not in (1,2) for r in rows)
        or any(b['task_id']<a['task_id'] for a,b in zip(rows,rows[1:]))):
        return dict(passed=False,reason='invalid task sequence')
    parts={str(k):assess_release([r for r in rows if r['task_id']==k],collisions) for k in (1,2)}
    return dict(passed=all(r['passed'] for r in parts.values()),tasks=parts,
        reason='two fixed lines and synthetic SDK lamp fields; real query body/task core; no actual CARLA lamps or official SDK')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--host', default='localhost');p.add_argument('--port', type=int, default=2000)
    p.add_argument('--speed-kmh', type=float, default=30)
    p.add_argument('--distance', type=float, default=60, help='Initial reference-to-line distance, m; map mode follows stop waypoint backwards')
    p.add_argument('--fixed-delta-seconds', type=float, default=0.05)
    p.add_argument('--seconds', type=float, default=45)
    p.add_argument('--release-after',type=float,help='External permission (map mode: set actual lamp green) after N simulation seconds')
    p.add_argument('--signal-tasks',action='store_true',
                   help='Two mandatory stops at distance/2 and distance; synthetic green at release-after and twice that time')
    p.add_argument('--map-signal',action='store_true',help='Use actual CARLA lamp and its API stop waypoint; ordinary signal response')
    p.add_argument('--initial-green',action='store_true',help='With --map-signal, keep actual lamp green from start')
    p.add_argument('--signal-profile',choices=('red-green','approach-red','approach-yellow','read-outage'),default='red-green',
                   help='Map mode: initially green, then red/yellow or injected failed read, then green/recovery at release-after')
    p.add_argument('--change-after',type=float,default=2,help='Dynamic profile changes after N simulation seconds')
    p.add_argument('--after-passage',choices=('red','read-outage'),
                   help='With initial-green, change the old lamp after observing the front across the line')
    p.add_argument('--distance-geometry', choices=('body','legacy'), default='body',
                   help='Measured body front, or old 4.38046m reference offset')
    p.add_argument('--stop-policy', choices=('approach','legacy'), default='approach',
                   help='Shared C++ gradual approach, or inherited full-brake trigger')
    p.add_argument('--no-realtime', action='store_true', help='Run as fast as the server allows')
    p.add_argument('--no-follow-camera', action='store_true', help='Leave the spectator camera under manual control')
    p.add_argument('--output', type=Path, default=Path('work/carla/stopline-baseline'))
    a = p.parse_args()
    if not all(math.isfinite(v) for v in (a.speed_kmh,a.distance,a.fixed_delta_seconds,a.seconds)) or not (0<a.speed_kmh<=50 and a.distance>10 and 0<a.fixed_delta_seconds<=0.1 and a.seconds>5):
        p.error('Invalid experiment parameters')
    if a.release_after is not None and (not math.isfinite(a.release_after) or not 5<a.release_after<a.seconds
            or a.stop_policy!='approach' or a.distance_geometry!='body'):
        p.error('Release requires approach/body and 5 < release-after < seconds')
    if a.signal_tasks and (a.release_after is None or 2*a.release_after>=a.seconds or a.distance<30):
        p.error('Signal tasks require distance >=30 and seconds > 2*release-after')
    if a.initial_green and (not a.map_signal or a.release_after is not None):
        p.error('Initial green requires --map-signal without --release-after')
    if a.map_signal and (a.signal_tasks or a.stop_policy!='approach' or a.distance_geometry!='body'
                         or (not a.initial_green and a.release_after is None)):
        p.error('Map signal requires approach/body and --release-after or --initial-green; excludes --signal-tasks')
    if a.signal_profile!='red-green' and (not a.map_signal or a.initial_green or a.release_after is None
            or not math.isfinite(a.change_after) or not 0<a.change_after<a.release_after):
        p.error('Dynamic profile requires --map-signal and 0 < change-after < release-after, without initial-green')
    if a.after_passage and not a.initial_green:
        p.error('After-passage requires --map-signal --initial-green')
    import carla
    a.output.mkdir(parents=True, exist_ok=True)
    if (a.output/'report.json').exists():
        p.error('Output already contains a report; choose a new --output')
    controller=StopLineController();steering=SteeringController();pedals=PISpeedActuators()
    sequence=(SignalStopTask(5) if a.signal_tasks else StopLineSequence(5)) if a.release_after is not None else None
    if a.map_signal:
        if sequence is not None: sequence.close()
        sequence=SignalStopTask(0)
    client=carla.Client(a.host,a.port);client.set_timeout(30)
    world=None;original=None;spectator=None;original_view=None;actors=[];rows=[];collisions=[];cleanup=[]
    lamp=None;lamp_original=[];original_frozen=None;lamp_switched=False;task_cleared=False
    signal_changed=False;last_observed_lamp=None
    front_crossed=False;passage_event=False
    report=dict(passed=False, sdk_verified=False, official_score_verified=False,
        source_build=controller.build, steering_build=steering.build,
        git_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=Path(__file__).resolve().parents[1],text=True).strip(),
        parameter_sha256=controller.parameter_sha256, parameters={k:str(v) if isinstance(v,Path) else v for k,v in vars(a).items()},
        script_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        scope='shared C++ stop-line control and optional mandatory-stop sequence; legacy bridge also runs nearest-line/Whole fragment; CARLA pedals/geometry; no complete drive or main_track',
        rule='p19 table22 stop subset: [0,0.5]m and >5s; optional separate p15 table12 departure check <=5s, synthetic permission; not official scoring')
    if a.signal_tasks:
        report['signal_scope']='real production SDK query body with synthetic field/return inputs; no real CARLA lamp selection'
    if a.map_signal:
        report['signal_scope']='actual CARLA actor state -> SDK field fixture -> real query body -> shared SignalTask(dwell=0); no full main_track'
        report['rule']='p15 table12: red stop gap [0,0.5]m and green departure <=5s subset; no compulsory dwell; API waypoint not verified painted line'
        report['adapter_sha256']=hashlib.sha256(Path(__file__).with_name('carla_signal.py').read_bytes()).hexdigest()
        report['lamp_cleanup_scope']='state/frozen flags restored and checked; phase elapsed time cannot be rewound'
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
        if a.map_signal:
            for candidate,stop,wp,path in approaches(world,a.distance):
                spawn=wp.transform
                spawn.location.z+=.5 # waypoints lie on asphalt; reserve body spawn clearance
                ego=world.try_spawn_actor(bp,prepare_ego_transform(carla,spawn,wp.transform,'lane-center'))
                if ego is not None:
                    lamp=candidate;forward=wp.transform.get_forward_vector();index=None
                    break
        for index,spawn in (() if a.map_signal else enumerate(road.get_spawn_points())):
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
        if a.map_signal:
            lamps=list(world.get_actors().filter('traffic.traffic_light'))
            lamp_original=[(x,x.get_state(),x.is_frozen()) for x in lamps]
            if len({frozen for _,_,frozen in lamp_original})!=1:
                raise RuntimeError('Mixed frozen lamp states: cannot safely restore global freeze')
            original_frozen=lamp_original[0][2]
            world.freeze_all_traffic_lights(True)
            for x,_,_ in lamp_original: x.set_state(carla.TrafficLightState.Red)
            if a.initial_green or a.signal_profile!='red-green': lamp.set_state(carla.TrafficLightState.Green)
            report['map_signal_binding']=dict(actor_id=lamp.id,road_id=stop.road_id,
                section_id=stop.section_id,lane_id=stop.lane_id,stop_s=stop.s,
                identity=[road.name,lamp.id,stop.road_id,stop.section_id,stop.lane_id,stop.s],
                token=1,stop_source='TrafficLight.get_stop_waypoints; painted marking equivalence unverified',
                affected_lanes=[(q.road_id,q.lane_id) for q in lamp.get_affected_lane_waypoints()])
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
        lines=[(origin.x+axis[0]*d,origin.y+axis[1]*d) for d in
               ((a.distance/2,a.distance) if a.signal_tasks else (a.distance,))]
        if a.map_signal: lines=[(stop.transform.location.x,stop.transform.location.y)]
        task_index=0;line=lines[0]
        formal_path=[(q.transform.location.x,-q.transform.location.y,q.transform.location.z) for q in path]
        report.update(platform=client.get_server_version(),map=road.name,vehicle=ego.type_id,
            spawn_index=index,road_id=wp.road_id,lane_id=wp.lane_id,line_xy=line,task_lines_xy=lines,
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
        start_simulation=world.get_snapshot().timestamp.elapsed_seconds
        release_shown=False
        for _ in range(math.ceil(a.seconds/a.fixed_delta_seconds)):
            frame_started=time.monotonic()
            line=lines[task_index]
            snapshot=world.get_snapshot();pose=ego.get_transform();location=pose.location
            speed=ego.get_velocity().length()
            reference=(line[0]-location.x)*axis[0]+(line[1]-location.y)*axis[1]
            vertices=ego.bounding_box.get_world_vertices(pose)
            front=max((v.x-location.x)*axis[0]+(v.y-location.y)*axis[1] for v in vertices)
            rear=min((v.x-location.x)*axis[0]+(v.y-location.y)*axis[1] for v in vertices)
            lateral=-(location.x-origin.x)*axis[1]+(location.y-origin.y)*axis[0]
            yaw=(pose.rotation.yaw-wp.transform.rotation.yaw+180)%360-180
            if abs(lateral)>0.4 or abs(yaw)>3: raise RuntimeError('Left straight experiment corridor')
            used_front=front if a.distance_geometry=='body' else 4.38046
            state=dict(phase=-1,held=0,ready=False)
            release_time=None if a.release_after is None else a.release_after*(task_index+1)
            release_allowed=release_time is not None and snapshot.timestamp.elapsed_seconds-start_simulation>=release_time
            lamp_name='synthetic'
            actual_lamp_name='synthetic';injected_fault=False
            if a.map_signal:
                elapsed=snapshot.timestamp.elapsed_seconds-start_simulation
                if a.signal_profile!='red-green' and elapsed>=a.change_after and not signal_changed:
                    if a.signal_profile=='approach-red': lamp.set_state(carla.TrafficLightState.Red)
                    elif a.signal_profile=='approach-yellow': lamp.set_state(carla.TrafficLightState.Yellow)
                    signal_changed=True
                if release_allowed and not lamp_switched:
                    lamp.set_state(carla.TrafficLightState.Green);lamp_switched=True
                if a.after_passage and front_crossed and not passage_event:
                    if a.after_passage=='red': lamp.set_state(carla.TrafficLightState.Red)
                    passage_event=True
                code,ok,lamp_name=observe(lamp)
                actual_lamp_name=lamp_name
                injected_fault=a.signal_profile=='read-outage' and a.change_after<=elapsed<a.release_after
                injected_fault=injected_fault or (a.after_passage=='read-outage' and passage_event)
                if injected_fault: code,ok,lamp_name=0,False,'injected_read_failure'
                release_allowed=ok and code==2 # permission comes from observation, NOT the timer
                if lamp_name!=last_observed_lamp:
                    print(f'Observed lamp: {lamp_name} at {elapsed:.2f}s.',flush=True)
                    last_observed_lamp=lamp_name
                # One binding for this run; token 1 is allocated only after the
                # map/lane/direction association, and is never an actor ID.
                state=sequence.step(1,snapshot.timestamp.elapsed_seconds,reference,used_front,speed,
                                    a.speed_kmh/3.6,controller.margin,code,ok=ok)
                target=state['target']
                front_crossed=reference-front<0
                if (release_allowed or state.get('passed',False)) and reference-front<=-5 and reference-rear<=0:
                    sequence.clear();task_cleared=True
            elif a.signal_tasks:
                state=sequence.step(task_index+1,snapshot.timestamp.elapsed_seconds,reference,used_front,speed,
                                    a.speed_kmh/3.6,controller.margin,2 if release_allowed else 1)
                target=state['target']
            elif sequence is not None:
                state=sequence.step(snapshot.timestamp.elapsed_seconds,reference,used_front,speed,
                                    a.speed_kmh/3.6,controller.margin,release_allowed)
                target=state['target']
            elif a.stop_policy=='legacy' and a.distance_geometry=='legacy':
                target=controller.step(reference,speed,a.speed_kmh/3.6)
            else:
                target=controller.step(reference,speed,a.speed_kmh/3.6,
                    front_offset=used_front,policy=a.stop_policy)
            throttle,brake=pedals.step(target,speed,a.fixed_delta_seconds)
            steer=steering.step(formal_path,*carla_to_formal(location.x,location.y,pose.rotation.yaw),speed)['command']
            rows.append(dict(frame=snapshot.frame,time=snapshot.timestamp.elapsed_seconds,speed_mps=speed,
                reference_distance_m=reference,front_offset_m=front,front_gap_m=reference-front,
                control_front_offset_m=used_front,target_mps=target,
                release_allowed=release_allowed,
                task_id=task_index+1,signal_state=state.get('signal',-1),
                lamp_state=lamp_name,rear_gap_m=reference-rear,task_cleared=task_cleared,
                actual_lamp_state=actual_lamp_name,injected_read_failure=injected_fault,
                signal_passed=state.get('passed',False),
                stop_phase=state['phase'],controller_held_s=state['held'],controller_ready=state['ready'],
                throttle=throttle,brake=brake,steering=steer,lateral_m=lateral))
            if release_allowed and not release_shown:
                print('Actual CARLA green observed; shared C++ signal controller permits departure.' if a.map_signal else
                      f'Task {task_index+1}: external permission granted; the C++ controller still requires completed standstill.',flush=True)
                world.debug.draw_line(carla.Location(line[0]-axis[1]*2,line[1]+axis[0]*2,origin.z+.03),
                    carla.Location(line[0]+axis[1]*2,line[1]-axis[0]*2,origin.z+.03),
                    thickness=.15,color=carla.Color(0,255,0),life_time=a.seconds)
                release_shown=True
            if (release_allowed or task_cleared) and reference-front<=-5 and (not a.map_signal or task_cleared):
                if task_index+1==len(lines): break # finish within the prevalidated corridor
                task_index+=1;release_shown=False
                next_line=lines[task_index]
                print(f'Task {task_index+1}: new stop line; dwell must be observed again.',flush=True)
                world.debug.draw_line(carla.Location(next_line[0]-axis[1]*2,next_line[1]+axis[0]*2,origin.z),
                    carla.Location(next_line[0]+axis[1]*2,next_line[1]-axis[0]*2,origin.z),
                    thickness=.1,color=carla.Color(255,0,0),life_time=a.seconds)
            ego.apply_control(carla.VehicleControl(throttle=throttle,brake=brake,steer=max(-1,min(1,steer)),manual_gear_shift=True,gear=1))
            world.tick(20)
            follow_camera()
            if not a.no_realtime:
                time.sleep(max(0.0,a.fixed_delta_seconds-(time.monotonic()-frame_started)))
        report['playback_wall_seconds']=time.monotonic()-playback_started
        report.update(assessment=assess_passage(rows,collisions,a.after_passage) if a.after_passage else
                      assess_transitions(rows,collisions,a.signal_profile) if a.map_signal and a.signal_profile!='red-green' else
                      assess_signal(rows,collisions,a.initial_green) if a.map_signal else
                      assess_signal_tasks(rows,collisions) if a.signal_tasks else
                      assess_release(rows,collisions) if sequence else assess(rows,collisions))
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
        if original_frozen is not None:
            try:
                for light,state,_ in lamp_original: light.set_state(state)
                world.freeze_all_traffic_lights(original_frozen)
                # State setters are RPCs; tick once while still synchronous to
                # read back restored state. Phase elapsed time cannot be reset.
                world.tick(20)
                if any(x.get_state()!=state or x.is_frozen()!=frozen for x,state,frozen in lamp_original):
                    cleanup.append('lamp restoration readback mismatch')
            except Exception as e: cleanup.append(f'lamp restore: {e}')
        if world is not None and original is not None:
            try: world.apply_settings(original)
            except Exception as e: cleanup.append(str(e))
        steering.close()
        if sequence is not None: sequence.close()
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
