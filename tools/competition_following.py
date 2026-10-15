"""Compile actual following/selection function bodies from competition sources.

Only SDK state types and logging are substituted. Map projection and full drive
execution are outside this bridge. Source/parameter hashes accompany each build.
"""
import ctypes
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ('competition/TrajectoryControl/src/core/obstacle.cpp',
           'competition/TrajectoryControl/src/tools/controller.hpp',
           'tools/native/following_bridge.cpp', 'tools/competition_following.py')
PARAM = ROOT / 'competition/TrajectoryControl/param.json'


def read_source(raw):
    try:
        text = raw.decode('utf-8-sig')
    except UnicodeDecodeError:
        text = raw.decode('gb18030')
    return text.replace('\r\n', '\n')


def extract(text, start, end):
    if text.count(start) != 1 or text.count(end) != 1:
        raise ValueError('Production function boundary changed; review bridge extraction')
    begin = text.index(start)
    return text[begin:text.index(end, begin)]


def build_library():
    if os.name != 'nt' or struct.calcsize('P') != 8:
        raise RuntimeError('Formal following bridge requires Windows x64 and MSVC')
    raw = {name: (ROOT / name).read_bytes() for name in SOURCES}
    hashes = {name: hashlib.sha256(value).hexdigest() for name, value in raw.items()}
    key = hashlib.sha256(json.dumps(hashes, sort_keys=True).encode()).hexdigest()[:20]
    output = ROOT / 'work/following_controller' / key
    binary, manifest = output / 'following.dll', output / 'build.json'
    if binary.exists() and manifest.exists():
        saved = json.loads(manifest.read_text(encoding='utf-8'))
        if saved['source_sha256'] == hashes and saved['binary_sha256'] == hashlib.sha256(binary.read_bytes()).hexdigest():
            return binary, saved
    production = read_source(raw[SOURCES[0]])
    sections = {
        '// @PRODUCTION_NEAREST@': extract(production, 'bool calculateNearestObstacle(', '// 找到所有在主车后方的障碍物'),
        '// @PRODUCTION_FOLLOWING@': extract(production, '// 计算跟车的速度', '// 计算超车速度'),
    }
    bridge = read_source(raw[SOURCES[2]])
    for marker, code in sections.items():
        if bridge.count(marker) != 1:
            raise ValueError('Bridge insertion point changed')
        bridge = bridge.replace(marker, code)
    output.mkdir(parents=True, exist_ok=True)
    (output / 'bridge.cpp').write_bytes(bridge.encode('utf-8'))
    (output / 'controller.hpp').write_bytes(read_source(raw[SOURCES[1]]).encode('utf-8'))
    vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    install = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
              'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
    if not install:
        raise RuntimeError('Install Visual Studio Desktop development with C++')
    vcvars = Path(install) / 'VC/Auxiliary/Build/vcvars64.bat'
    result = subprocess.run(f'call "{vcvars}" >nul && set', shell=True, capture_output=True, check=True)
    env = os.environ.copy()
    for line in result.stdout.decode('mbcs').splitlines():
        name, sep, value = line.partition('=')
        if sep and name and not name.startswith('='):
            env[name] = value
    compiler = Path(env['VCToolsInstallDir']) / 'bin/Hostx64/x64/cl.exe'
    result = subprocess.run([str(compiler), '/nologo', '/LD', '/EHsc', '/std:c++17', '/utf-8',
                             '/W4', '/O2', '/MT', 'bridge.cpp', f'/Fe:{binary}'],
                            cwd=output, env=env, capture_output=True, timeout=120)
    (output / 'compile.log').write_bytes(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f'Following bridge compile failed; see {output / "compile.log"}')
    report = dict(source_sha256=hashes, binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                  extracted_sha256={name: hashlib.sha256(code.encode()).hexdigest() for name, code in sections.items()},
                  toolset=env['VCToolsVersion'].strip(), abi_version=3, sdk_build_verified=False,
                  scope='production nearest-obstacle and following-speed bodies plus real AdaptiveFollowing/PID')
    manifest.write_text(json.dumps(report, indent=2), encoding='utf-8')
    return binary, report


class _Input(ctypes.Structure):
    _fields_ = [('s', ctypes.c_double), ('speed', ctypes.c_double), ('type', ctypes.c_int),
                ('same_lane', ctypes.c_int), ('same_road', ctypes.c_int)]


class _Output(ctypes.Structure):
    _fields_ = [('target', ctypes.c_double), ('selected_s', ctypes.c_double),
                ('distance', ctypes.c_double), ('found', ctypes.c_int)]


class FollowingController:
    def __init__(self):
        binary, self.build = build_library()
        params_raw = PARAM.read_bytes()
        params = json.loads(params_raw.decode('utf-8-sig'))
        names = ('followingKp', 'followingKi', 'followingKd', 'followingLimitSpeed', 'caseStop')
        self.parameters = {name: params[name] for name in names}
        if not all(math.isfinite(float(v)) for v in self.parameters.values()):
            raise ValueError('Following parameters must be finite')
        self.parameter_sha256 = hashlib.sha256(params_raw).hexdigest()
        self._dll = ctypes.CDLL(str(binary))
        self._dll.following_abi_version.restype = ctypes.c_int
        if self._dll.following_abi_version() != 3:
            raise RuntimeError('Following ABI version mismatch')
        self._dll.following_create.argtypes = [ctypes.c_float]*4 + [ctypes.c_int]
        self._dll.following_create.restype = ctypes.c_void_p
        self._dll.following_destroy.argtypes = [ctypes.c_void_p]
        self._dll.following_destroy.restype = None
        self._dll.following_reset.argtypes = [ctypes.c_void_p]
        self._dll.following_reset.restype = ctypes.c_int
        self._dll.following_step.argtypes = [ctypes.c_void_p, ctypes.c_double, ctypes.c_double,
                ctypes.POINTER(_Input), ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_double,
                ctypes.c_int, ctypes.c_double, ctypes.c_double, ctypes.c_double,
                ctypes.POINTER(_Output)]
        self._dll.following_step.restype = ctypes.c_int
        self._handle = self._dll.following_create(*(self.parameters[name] for name in names))
        if not self._handle:
            raise ValueError('Invalid following configuration or allocation failure')

    def step(self, ego_s, ego_speed, obstacles, case_index=2, *, static_reference_offset=None, policy=None):
        """Run the production controller; rejected input clears history before raising.

        This exception is not a vehicle stop command. The caller must request a
        safe stop or terminate its experiment before accepting another frame.
        """
        try:
            return self._step(ego_s, ego_speed, obstacles, case_index,
                              static_reference_offset=static_reference_offset, policy=policy)
        except (ValueError, TypeError, OverflowError, ctypes.ArgumentError):
            self.reset()
            raise

    def reset(self):
        """Clear PID history for this controller only; retain its configured gains."""
        if not getattr(self, '_handle', None) or not self._dll.following_reset(self._handle):
            raise RuntimeError('Following controller is closed')

    def _step(self, ego_s, ego_speed, obstacles, case_index=2, *, static_reference_offset=None, policy=None):
        """Obstacles: (lane-s, speed m/s, type 0..3, same-lane, same-road).

        lane-s is an absolute coordinate on the ego lane, not signed gap. Type 0
        is static; callers must provide semantic classification, not raw SDK IDs.
        static_reference_offset optionally supplies reference-to-body geometry for
        the selected static target, in metres along the same axis. None preserves
        the legacy SimOne value. With explicit policy, this same measured offset
        also converts the moving target's distance to body clearance. policy keys
        are moving_gap_m, stopped_gap_m, max_speed_mps; no case-ID inference occurs.
        """
        if not getattr(self, '_handle', None):
            raise RuntimeError('Following controller is closed')
        if len(obstacles) > 10000:
            raise ValueError('Too many obstacles for the bridge')
        if static_reference_offset is not None and (not math.isfinite(static_reference_offset) or static_reference_offset < 0):
            raise ValueError('Static reference offset must be finite and nonnegative')
        policy_values = (0.0, 0.0, 0.0)
        if policy is not None:
            keys = ('moving_gap_m', 'stopped_gap_m', 'max_speed_mps')
            if static_reference_offset is None or set(policy) != set(keys):
                raise ValueError('Explicit policy requires measured reference offset and all three policy fields')
            policy_values = tuple(policy[k] for k in keys)
            if (not all(math.isfinite(v) for v in policy_values) or policy_values[0] <= 0
                    or policy_values[1] <= 0 or policy_values[2] < 0):
                raise ValueError('Policy gaps must be positive and ceiling nonnegative; all must be finite')
        data = (_Input * len(obstacles))(*(_Input(*item) for item in obstacles))
        result = _Output()
        if not self._dll.following_step(self._handle, ego_s, ego_speed, data, len(data), case_index,
                static_reference_offset is not None, static_reference_offset or 0,
                policy is not None, *policy_values, ctypes.byref(result)):
            raise ValueError('Invalid following input or non-finite production output')
        return dict(target_speed=result.target, selected_s=result.selected_s if result.found else None,
                    selected_distance=result.distance if result.found else None)

    def close(self):
        if getattr(self, '_handle', None):
            self._dll.following_destroy(self._handle)
            self._handle = None

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    def __del__(self):
        self.close()
