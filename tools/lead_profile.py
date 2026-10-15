"""Deterministic auxiliary lead motion and independent stop/go acceptance.

Scoring PDF physical p22 table27 supplies stop/go intent and 10..50m gap limits.
Profile speed, braking, hold duration and acceptance tolerances are experimental.
The lead is a kinematic stimulus, not a simulated braking-performance model.
"""
import math
from dataclasses import dataclass


@dataclass(frozen=True)
class BrakingProfile:
    """Scoring PDF p11 table4 stimulus; hold duration is experimental.

    A kinematic lead follows 30km/h for 3s then decelerates at 2m/s^2.
    It stays stopped; restart belongs to the separate stop-go scenario.
    """
    speed: float = 30 / 3.6
    cruise_seconds: float = 3.0
    deceleration: float = 2.0
    hold_seconds: float = 30.0

    def __post_init__(self):
        if not all(math.isfinite(v) and v > 0 for v in vars(self).values()):
            raise ValueError('Profile values must be finite and positive')

    @property
    def stop_time(self):
        return self.cruise_seconds + self.speed / self.deceleration

    @property
    def end_time(self):
        return self.stop_time + self.hold_seconds

    def sample(self, seconds):
        if not math.isfinite(seconds) or seconds < 0:
            raise ValueError('Profile time must be finite and nonnegative')
        if seconds < self.cruise_seconds:
            return self.speed * seconds, self.speed, 'cruise'
        braking_time = min(seconds - self.cruise_seconds, self.speed / self.deceleration)
        distance = self.speed * self.cruise_seconds + self.speed * braking_time - 0.5 * self.deceleration * braking_time**2
        return distance, max(0.0, self.speed - self.deceleration * braking_time), ('braking' if seconds < self.stop_time else 'holding')


def assess_braking(samples, profile, initial_gap=50.0, stop_gap=2.0, dwell_seconds=2.0,
                   exclusive=False, max_gap_seconds=0.25):
    """Independent engineering audit, NOT a score or full scenario reproduction.

    Speed <=0.1m/s, final gap +/-0.5m, cruise speed +/-0.5m/s and gap
    +/-0.5m are experimental tolerances. The rule actually deducts 50 points
    for a 1..3.5m stopping gap. Never label that band as a full-score pass.
    """
    valid = bool(samples)
    held = 0.0
    previous = None
    speed_error = gap_error = 0.0
    cruise_seen = False
    overlap = False
    for row in samples:
        t, speed, gap, target = (row[k] for k in ('elapsed', 'speed_mps', 'gap_m', 'formal_target_speed_mps'))
        if (not all(math.isfinite(v) for v in (t, speed, gap, target)) or t < 0 or speed < 0 or target < 0
                or (previous is not None and not 0 < t - previous['elapsed'] <= max_gap_seconds)):
            valid = False
            break
        overlap = overlap or gap <= 0
        if t <= profile.cruise_seconds:
            cruise_seen = True
            speed_error = max(speed_error, abs(speed - profile.speed))
            gap_error = max(gap_error, abs(gap - initial_gap))
        stopped = (t >= profile.stop_time and speed <= 0.1 and abs(gap - stop_gap) <= 0.5 and target == 0)
        held = held + t - previous['elapsed'] if stopped and previous is not None and previous['stopped'] else 0.0
        previous = dict(elapsed=t, stopped=stopped)
    complete = valid and samples[0]['elapsed'] <= max_gap_seconds and samples[-1]['elapsed'] >= profile.end_time
    cruise_matched = valid and cruise_seen and speed_error <= 0.5 and gap_error <= 0.5
    stop_ok = complete and not overlap and (held > dwell_seconds if exclusive else held >= dwell_seconds)
    reasons = []
    if not complete:
        reasons.append('Incomplete or invalid measured timeline')
    if overlap:
        reasons.append('Body overlap detected')
    if not cruise_matched:
        reasons.append('Initial cruise speed/gap departed from experiment tolerances')
    if not stop_ok:
        reasons.append('Final continuous standstill/clearance/zero-command requirement failed')
    return dict(passed=stop_ok and cruise_matched, stop_passed=stop_ok,
                cruise_matched=cruise_matched, complete=complete, valid_timeline=valid,
                cruise_max_speed_error_mps=speed_error, cruise_max_gap_error_m=gap_error,
                final_standstill_seconds=held, body_overlap=overlap,
                error='; '.join(reasons) if reasons else None)


@dataclass(frozen=True)
class StopGoProfile:
    speed: float = 3.0
    cruise_seconds: float = 10.0
    deceleration: float = 2.0
    hold_seconds: float = 10.0
    acceleration: float = 1.0
    recovery_seconds: float = 5.0

    def __post_init__(self):
        if not all(math.isfinite(v) and v > 0 for v in vars(self).values()):
            raise ValueError('Profile values must be finite and positive')

    @property
    def stop_time(self):
        return self.cruise_seconds + self.speed / self.deceleration

    @property
    def restart_time(self):
        return self.stop_time + self.hold_seconds

    @property
    def end_time(self):
        return self.restart_time + self.speed / self.acceleration + self.recovery_seconds

    def sample(self, seconds):
        if not math.isfinite(seconds) or seconds < 0:
            raise ValueError('Profile time must be finite and nonnegative')
        t = seconds
        if t < self.cruise_seconds:
            return self.speed*t, self.speed, 'cruise'
        s = self.speed*self.cruise_seconds
        t -= self.cruise_seconds
        braking = self.speed/self.deceleration
        if t < braking:
            return s+self.speed*t-0.5*self.deceleration*t*t, self.speed-self.deceleration*t, 'braking'
        s += self.speed*braking/2
        t -= braking
        if t < self.hold_seconds:
            return s, 0.0, 'holding'
        t -= self.hold_seconds
        rising = self.speed/self.acceleration
        if t < rising:
            return s+0.5*self.acceleration*t*t, self.acceleration*t, 'restarting'
        s += self.speed*rising/2
        return s+self.speed*(t-rising), self.speed, 'recovered'


def assess_stop_go(samples, profile, max_gap_seconds=0.25):
    """Audit a complete measured timeline; never infer dwell across missing data."""
    if not samples:
        return dict(passed=False, error='No measured frames')
    longest = held = 0.0
    previous = None
    recovery_seen = False
    gap_ok = valid = True
    for row in samples:
        t, speed, gap = (row[k] for k in ('elapsed', 'speed_mps', 'gap_m'))
        if not all(math.isfinite(v) for v in (t, speed, gap)) or t < 0 or speed < 0:
            valid = False
            break
        if previous is not None and not 0 < t-previous['elapsed'] <= max_gap_seconds:
            valid = False
            break
        gap_ok = gap_ok and 10 <= gap <= 50
        phase = profile.sample(t)[2]
        stopped = phase == 'holding' and speed <= 0.1
        if stopped and previous is not None and previous['stopped']:
            held += t-previous['elapsed']
        else:
            held = 0
        longest = max(longest, held)
        if phase == 'recovered' and speed >= 1.0:
            recovery_seen = True
        previous = dict(elapsed=t, stopped=stopped)
    complete = valid and samples[0]['elapsed'] <= max_gap_seconds and samples[-1]['elapsed'] >= profile.end_time
    passed = complete and gap_ok and longest >= 2 and recovery_seen
    return dict(passed=passed, complete=complete, valid_timeline=valid,
                gap_in_range=gap_ok, simultaneous_standstill_seconds=longest,
                resumed=recovery_seen,
                error=None if passed else 'Incomplete/invalid timeline, gap violation, insufficient stop or no restart')
