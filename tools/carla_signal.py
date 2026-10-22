"""Bounded straight-approach CARLA adapter; no SimOne/map-marking equivalence.

get_stop_waypoints identifies approach lanes. get_affected_lane_waypoints may
identify junction lanes instead and must not be used as approach lane IDs.
"""
import math


def lane_key(wp):
    return wp.road_id, wp.section_id, wp.lane_id


def along(origin, point):
    f = origin.transform.get_forward_vector()
    a, b = origin.transform.location, point.transform.location
    return (b.x-a.x)*f.x+(b.y-a.y)*f.y


def on_straight(origin, point):
    a, b = origin.transform.location, point.transform.location
    f = origin.transform.get_forward_vector()
    yaw = (point.transform.rotation.yaw-origin.transform.rotation.yaw+180)%360-180
    return (all(math.isfinite(v) for v in (a.x,a.y,a.z,b.x,b.y,b.z,f.x,f.y,yaw))
            and abs(yaw) <= .5 and abs(-(b.x-a.x)*f.y+(b.y-a.y)*f.x) <= .1
            and abs(b.z-a.z) <= .2)


def select_binding(start, bindings):
    """Only accept one nearest forward stop on this approach lane/direction.

    Multiple actors for the same stop are deliberately ambiguous until their
    group semantics are implemented. Never select a lamp by pole proximity.
    """
    candidates = [(along(start,stop),light,stop) for light,stop in bindings
                  if lane_key(start)==lane_key(stop) and on_straight(start,stop)
                  and along(start,stop)>0]
    candidates.sort(key=lambda item:item[0])
    if not candidates:
        return None
    if len(candidates)>1 and candidates[1][0]-candidates[0][0] < .5:
        return None
    return candidates[0][1:]


def approaches(world, distance):
    """Enumerate map-derived, validated short straight experiments only."""
    bindings = [(light,stop) for light in world.get_actors().filter('traffic.traffic_light')
                for stop in light.get_stop_waypoints()]
    for light,stop in bindings:
        previous = stop.previous(distance)
        if len(previous)!=1:
            continue
        start=previous[0]
        binding=select_binding(start,bindings)
        if binding is None or binding[0].id!=light.id or binding[1].id!=stop.id:
            continue
        path=[start]
        for d in range(2,math.ceil(distance+20)+1,2):
            candidates=[q for q in start.next(d) if on_straight(start,q)
                        and (d>distance or lane_key(q)==lane_key(start))]
            if not candidates:
                break
            # Coincident junction waypoints can belong to different turns. This
            # experiment follows only the checked straight geometry for 5m past
            # the front; it does not select a complete intersection route.
            path.append(candidates[0])
        if len(path)==len(range(2,math.ceil(distance+20)+1,2))+1:
            yield light,stop,start,path


def observe(light):
    """Bridge codes are explicit strings, never CARLA/SimOne enum ordinals."""
    try:
        if not light.is_alive:
            return 0,False,'missing'
        name=str(light.get_state())
        code={'Red':1,'Green':2,'Yellow':3}.get(name,0)
        return code,code!=0,name
    except RuntimeError:
        return 0,False,'read_failed'


def assess_signal(rows, collisions, initially_green=False):
    """Independent p15 table12-inspired checks, without a mandatory 5s dwell.

    Gap is measured to the CARLA API stop waypoint, NOT a verified painted line.
    """
    if (not rows or collisions or any(not all(math.isfinite(r[k]) for k in
            ('time','speed_mps','front_gap_m','target_mps','rear_gap_m')) for r in rows)
            or any(not 0<b['time']-a['time']<=.25 for a,b in zip(rows,rows[1:]))):
        return dict(passed=False,reason='collision, missing or invalid samples')
    first=next((i for i,r in enumerate(rows) if r['lamp_state']=='Green'),None)
    if first is None or any(r['lamp_state']!='Green' for r in rows[first:]):
        return dict(passed=False,reason='no continuous observed green')
    before,after=rows[:first],rows[first:]
    clear=after[-1]['front_gap_m']<=-5 and after[-1]['rear_gap_m']<=0 and after[-1]['task_cleared']
    if initially_green:
        direct=(first==0 and all(r['target_mps']>0 for r in rows)
                and all(r['speed_mps']>.1 for r in rows if r['front_gap_m']<10))
        return dict(passed=direct and clear,unnecessary_stop=not direct,cleared=clear,
                    reason='actual CARLA lamp, API stop point; no official scoring')
    if not before or any(r['lamp_state'] not in ('Red','Yellow') for r in before):
        return dict(passed=False,reason='missing or invalid stop signal')
    gap=before[-1]['front_gap_m']
    stopped=before[-1]['speed_mps']<=.1 and before[-1]['target_mps']==0
    crossed=any(r['front_gap_m']<0 for r in before)
    moved=next((r for r in after if r['speed_mps']>.1),None)
    delay=None if moved is None else moved['time']-after[0]['time']
    return dict(passed=stopped and not crossed and 0<=gap<=.5 and delay is not None and delay<=5 and clear,
                final_red_gap_m=gap,stopped_on_red=stopped,crossed_on_red=crossed,
                departure_delay_s=delay,cleared=clear,
                reason='p15 table12 response subset; CARLA API stop point, no painted-line/official scoring validation')


def assess_transitions(rows,collisions,profile):
    """Require actual motion, an observed stop/fault interval, then recovery.

    The fault test uses an explicitly injected failed read, not a CARLA outage.
    Stopping early on unknown input is permitted; the red/yellow distance check
    is not applied to that different engineering failure mode.
    """
    expected={'approach-red':'Red','approach-yellow':'Yellow',
              'read-outage':'injected_read_failure'}.get(profile)
    if (expected is None or not rows or collisions
            or any(not all(math.isfinite(r[k]) for k in
                ('time','speed_mps','front_gap_m','target_mps','rear_gap_m')) for r in rows)
            or any(not 0<b['time']-a['time']<=.25 for a,b in zip(rows,rows[1:]))):
        return dict(passed=False,reason='invalid profile, collision or interrupted samples')
    begin=next((i for i,r in enumerate(rows) if r['lamp_state']!= 'Green'),None)
    if begin is None or begin==0 or rows[begin-1]['speed_mps']<=.1:
        return dict(passed=False,reason='no signal change while moving')
    end=next((i for i in range(begin,len(rows)) if rows[i]['lamp_state']=='Green'),None)
    if end is None or any(r['lamp_state']!=expected for r in rows[begin:end]) or any(
            r['lamp_state']!='Green' for r in rows[end:]):
        return dict(passed=False,reason='unexpected signal/fault sequence or no recovery')
    if expected!='injected_read_failure':
        # Existing independent red-stop + departure check accepts yellow as a
        # conservative stop request. Signal onset was verified above.
        result=assess_signal(rows[begin:],collisions)
        result['initial_motion_verified']=True
        result['profile']=profile
        return result
    fault=rows[begin:end];after=rows[end:]
    moved=next((r for r in after if r['speed_mps']>.1),None)
    delay=None if moved is None else moved['time']-after[0]['time']
    safe=all(r['target_mps']==0 and r['front_gap_m']>=0 for r in fault)
    stopped=fault[-1]['speed_mps']<=.1
    cleared=after[-1]['task_cleared'] and after[-1]['front_gap_m']<=-5 and after[-1]['rear_gap_m']<=0
    return dict(passed=safe and stopped and delay is not None and delay<=5 and cleared,
        fault_stop_gap_m=fault[-1]['front_gap_m'],zero_request_during_fault=safe,
        stopped_during_fault=stopped,departure_delay_s=delay,cleared=cleared,
        reason='injected adapter read failure with real CARLA physics; <=5s recovery engineering target, not official fault scoring')
