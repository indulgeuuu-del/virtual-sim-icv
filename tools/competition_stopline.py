"""Fixed-line experiment: actual nearest-line selection and Whole braking body.

Inputs are straight-road reference distance and speed, not SDK objects.
Legacy calls retain the inherited offset/polynomial; explicit calls use measured
geometry and optionally the shared C++ approach speed envelope.
"""
import ctypes
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess

try:
    from .competition_following import read_source, extract
except ImportError:
    from competition_following import read_source, extract

ROOT = Path(__file__).resolve().parents[1]


def build_library():
    paths = ['competition/TrajectoryControl/src/core/stopline.cpp',
             'competition/TrajectoryControl/src/main_track.cpp',
             'tools/native/stopline_bridge.cpp', 'tools/competition_stopline.py',
             'competition/util/StopLineControl.h']
    raw = {p: (ROOT / p).read_bytes() for p in paths}
    hashes = {p: hashlib.sha256(b).hexdigest() for p, b in raw.items()}
    out = ROOT / 'work/stopline_controller' / hashlib.sha256(json.dumps(hashes, sort_keys=True).encode()).hexdigest()[:20]
    dll, meta = out / 'stopline.dll', out / 'build.json'
    if dll.exists() and meta.exists():
        saved = json.loads(meta.read_text())
        if saved['source_sha256'] == hashes and saved['binary_sha256'] == hashlib.sha256(dll.read_bytes()).hexdigest():
            return dll, saved
    code = read_source(raw[paths[2]])
    nearest = extract(read_source(raw[paths[0]]), 'bool calculateNearestStopLine(', '// 默认构造函数')
    brake = extract(read_source(raw[paths[1]]),
                    'if (potentialStopLine.type == StopLine::Type::Whole &&', '/* 半停止线，主车的策略是变道 */')
    code = code.replace('// @NEAREST@', nearest).replace('// @BRAKING@', brake)
    code = code.replace('// @STOP_CONTROL@', read_source(raw[paths[4]]))
    out.mkdir(parents=True, exist_ok=True)
    (out / 'bridge.cpp').write_text(code, encoding='utf-8')
    vs = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    install = subprocess.check_output([str(vs), '-latest', '-products', '*', '-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
    if not install:
        raise RuntimeError('Install Visual Studio Desktop development with C++')
    vc = Path(install) / 'VC/Auxiliary/Build/vcvars64.bat'
    result = subprocess.run(f'call "{vc}" >nul && set', shell=True, capture_output=True, check=True)
    env = os.environ.copy()
    for line in result.stdout.decode('mbcs').splitlines():
        key, sep, value = line.partition('=')
        if sep and key and not key.startswith('='):
            env[key] = value
    compiler = Path(env['VCToolsInstallDir']) / 'bin/Hostx64/x64/cl.exe'
    result = subprocess.run([str(compiler), '/nologo', '/LD', '/EHsc', '/std:c++17', '/utf-8',
        '/O2', '/MT', 'bridge.cpp', '/Fe:stopline.dll'], cwd=out, env=env, capture_output=True, timeout=120)
    (out / 'compile.log').write_bytes(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f'Build failed: {out / "compile.log"}')
    report = dict(source_sha256=hashes, binary_sha256=hashlib.sha256(dll.read_bytes()).hexdigest(),
        sdk_build_verified=False, scope='shared C++ legacy/approach control; legacy entry also executes nearest-line and Whole fragment; no constructors, map, drive or arbitration')
    meta.write_text(json.dumps(report, indent=2), encoding='utf-8')
    return dll, report


class StopLineController:
    def __init__(self):
        dll, self.build = build_library()
        self.dll = ctypes.CDLL(str(dll))
        self.dll.stopline_step.argtypes = [ctypes.c_double]*4
        self.dll.stopline_step.restype = ctypes.c_double
        self.dll.stopline_explicit.argtypes = [ctypes.c_double]*5 + [ctypes.c_int]
        self.dll.stopline_explicit.restype = ctypes.c_double
        param = ROOT / 'competition/TrajectoryControl/param.json'
        self.parameter_sha256 = hashlib.sha256(param.read_bytes()).hexdigest()
        self.margin = json.loads(read_source(param.read_bytes()))['stopLineDistThres']

    def step(self, reference_distance, speed, cruise, *, front_offset=None, policy='legacy'):
        if not all(math.isfinite(v) for v in (reference_distance, speed, cruise, self.margin)) or min(speed, cruise, self.margin) < 0:
            raise ValueError('Invalid stop-line state')
        if policy not in ('legacy','approach') or (policy == 'approach' and front_offset is None):
            raise ValueError('Approach requires explicit geometry and a known policy')
        if front_offset is not None:
            if not math.isfinite(front_offset) or front_offset < 0:
                raise ValueError('Invalid reference-to-front offset')
            result = self.dll.stopline_explicit(reference_distance,speed,cruise,self.margin,front_offset,policy=='approach')
        else:
            result = self.dll.stopline_step(reference_distance, speed, cruise, self.margin)
        if not math.isfinite(result) or result < 0:
            raise RuntimeError('Invalid native stop-line output')
        return result
