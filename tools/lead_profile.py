"""Deterministic auxiliary lead motion and independent stop/go acceptance.

Scoring PDF physical p22 table27 supplies stop/go intent and 10..50m gap limits.
Profile speed, braking, hold duration and acceptance tolerances are experimental.
The lead is a kinematic stimulus, not a simulated braking-performance model.
"""
import math
from dataclasses import dataclass


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
