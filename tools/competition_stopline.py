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
             'competition/util/StopLineControl.h', 'competition/util/StopController.h',
             'competition/TrajectoryControl/src/core/process.cpp', 'tests/cpp/traffic_light_test.cpp']
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
    query = extract(read_source(raw[paths[6]]), 'bool getValidTrafficLight(', '// 判断主车轨迹是否涉及变道过程')
    boundary = read_source(raw[paths[7]]).split('// @TRAFFIC_LIGHT@',1)[0]
    code = code.replace('// @TRAFFIC_QUERY@',boundary + query)
    out.mkdir(parents=True, exist_ok=True)
    (out / 'StopController.h').write_bytes(raw[paths[5]])
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
        sdk_build_verified=False, scope='shared C++ stop/task control and production lamp query with SDK field fixtures; legacy entry also executes nearest-line/Whole fragment; no constructors, map, drive or arbitration')
    meta.write_text(json.dumps(report, indent=2), encoding='utf-8')
    return dll, report


class _SequenceOutput(ctypes.Structure):
    _fields_ = [('target',ctypes.c_double),('held',ctypes.c_double),
                ('phase',ctypes.c_int),('ready',ctypes.c_int)]


class StopLineSequence:
    """One mandatory stop. Permission is external, never inferred from elapsed time."""
    def __init__(self, dwell_seconds=5):
        dll,self.build=build_library()
        self.dll=ctypes.CDLL(str(dll))
        self.dll.sequence_create.argtypes=[ctypes.c_double]
        self.dll.sequence_create.restype=ctypes.c_void_p
        self.dll.sequence_destroy.argtypes=[ctypes.c_void_p]
        self.dll.sequence_destroy.restype=None
        self.dll.sequence_step.argtypes=[ctypes.c_void_p]+[ctypes.c_double]*6+[ctypes.c_int]*2+[ctypes.POINTER(_SequenceOutput)]
        self.dll.sequence_step.restype=ctypes.c_int
        self.handle=self.dll.sequence_create(dwell_seconds)
        if not self.handle:
            raise ValueError('Invalid dwell duration or allocation failure')

    def step(self,time,distance,front,speed,ceiling,margin,permission=False,observed=True):
        if not self.handle:
            raise RuntimeError('Closed stop-line sequence')
        out=_SequenceOutput()
        if not self.dll.sequence_step(self.handle,time,distance,front,speed,ceiling,margin,
                                      bool(permission),bool(observed),ctypes.byref(out)):
            raise RuntimeError('Stop-line sequence call failed')
        return dict(target=out.target,held=out.held,phase=out.phase,ready=bool(out.ready))

    def close(self):
        if getattr(self,'handle',None):
            self.dll.sequence_destroy(self.handle)
            self.handle=None

    def __del__(self):
        self.close()


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


class SignalStopTask:
    """Real SDK query body with field-level fixture inputs, then shared task logic.

    status: -1 absent, 0 unsupported, 1 red, 2 green, 3 yellow.
    These are bridge codes, never assumed to equal SDK enum numbers.
    dwell=0 is plain signal response; dwell>0 explicitly assigns mandatory hold.
    """
    def __init__(self,dwell_seconds=0):
        dll,self.build=build_library()
        self.dll=ctypes.CDLL(str(dll))
        self.dll.signal_create.argtypes=[ctypes.c_double]
        self.dll.signal_create.restype=ctypes.c_void_p
        self.dll.signal_destroy.argtypes=[ctypes.c_void_p]
        self.dll.signal_destroy.restype=None
        self.dll.signal_clear.argtypes=[ctypes.c_void_p]
        self.dll.signal_clear.restype=None
        self.dll.signal_step.argtypes=[ctypes.c_void_p,ctypes.c_int]+[ctypes.c_double]*6+[ctypes.c_int]*3+[ctypes.POINTER(_SequenceOutput),ctypes.POINTER(ctypes.c_int)]
        self.dll.signal_step.restype=ctypes.c_int
        self.handle=self.dll.signal_create(dwell_seconds)
        if not self.handle: raise ValueError('Invalid dwell or allocation failure')

    def step(self,token,time,distance,front,speed,ceiling,margin,status,ok=True,relevant=True):
        if not self.handle: raise RuntimeError('Closed signal task')
        if not isinstance(token,int) or not 0<=token<=2147483647 or status not in (-1,0,1,2,3):
            raise ValueError('Invalid task token or signal bridge code')
        out=_SequenceOutput();signal=ctypes.c_int()
        if not self.dll.signal_step(self.handle,token,time,distance,front,speed,ceiling,margin,
            status,bool(ok),bool(relevant),ctypes.byref(out),ctypes.byref(signal)):
            raise RuntimeError('Signal task call failed')
        return dict(target=out.target,held=out.held,phase=out.phase,ready=bool(out.ready),signal=signal.value)

    def clear(self):
        if not self.handle: raise RuntimeError('Closed signal task')
        self.dll.signal_clear(self.handle)

    def close(self):
        if getattr(self,'handle',None):
            self.dll.signal_destroy(self.handle);self.handle=None

    def __del__(self): self.close()
