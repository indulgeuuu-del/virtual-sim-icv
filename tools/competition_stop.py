"""Build/load the repository's C++ stop controller through a versioned C ABI.

Windows x64 + MSVC + Python 3.10. No SimOne or CARLA needed to build/test.
Outputs are content-addressed under work/stop_controller; nothing is installed globally.
"""
import ctypes
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
INPUTS = ('competition/util/StopController.h', 'tools/native/stop_bridge.cpp', 'tools/competition_stop.py')
PHASES = ('approaching', 'stopping', 'holding', 'ready', 'invalid')


def build_library():
    if os.name != 'nt' or struct.calcsize('P') != 8:
        raise RuntimeError('C++ bridge currently requires 64-bit Windows Python and MSVC')
    hashes = {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in INPUTS}
    key = hashlib.sha256(json.dumps(hashes, sort_keys=True).encode()).hexdigest()[:20]
    output = ROOT / 'work/stop_controller' / key
    binary, manifest = output / 'stop_controller.dll', output / 'build.json'
    if binary.exists() and manifest.exists():
        previous = json.loads(manifest.read_text(encoding='utf-8'))
        if (previous.get('source_sha256') == hashes and
                previous.get('binary_sha256') == hashlib.sha256(binary.read_bytes()).hexdigest()):
            return binary, previous
    output.mkdir(parents=True, exist_ok=True)
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
                             '/W4', '/WX', '/O2', '/MT', f'/I{ROOT / "competition/util"}',
                             str(ROOT / 'tools/native/stop_bridge.cpp'), f'/Fe:{binary}'],
                            cwd=output, env=env, capture_output=True, timeout=120)
    (output / 'compile.log').write_bytes(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f'C++ bridge compile failed; see {output / "compile.log"}')
    report = {'source_sha256': hashes, 'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
              'toolset': env['VCToolsVersion'].strip(), 'abi_version': 1, 'sdk_build_verified': False}
    manifest.write_text(json.dumps(report, indent=2), encoding='utf-8')
    return binary, report


class _Output(ctypes.Structure):
    _fields_ = [('target', ctypes.c_double), ('held', ctypes.c_double),
                ('phase', ctypes.c_int), ('ready', ctypes.c_int)]


@dataclass(frozen=True)
class StopResult:
    target_speed: float
    held_seconds: float
    phase: str
    ready: bool


class StopController:
    def __init__(self, dwell_seconds, exclusive=False, stationary_speed=0.1, max_sample_gap=0.25):
        binary, self.build = build_library()
        self.library_path = binary
        self._dll = ctypes.CDLL(str(binary))
        self._dll.stop_abi_version.restype = ctypes.c_int
        if self._dll.stop_abi_version() != 1:
            raise RuntimeError('Stop controller ABI version mismatch')
        self._dll.stop_create.argtypes = [ctypes.c_double, ctypes.c_int, ctypes.c_double, ctypes.c_double]
        self._dll.stop_create.restype = ctypes.c_void_p
        self._dll.stop_destroy.argtypes = [ctypes.c_void_p]
        self._dll.stop_destroy.restype = None
        self._dll.stop_reset.argtypes = [ctypes.c_void_p, ctypes.c_double, ctypes.c_int]
        self._dll.stop_reset.restype = ctypes.c_int
        self._dll.stop_step.argtypes = [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_int,
                                       ctypes.c_double, ctypes.c_int, ctypes.c_int, ctypes.POINTER(_Output)]
        self._dll.stop_step.restype = ctypes.c_int
        self._handle = self._dll.stop_create(dwell_seconds, int(exclusive), stationary_speed, max_sample_gap)
        if not self._handle:
            raise ValueError('Invalid stop controller configuration or allocation failure')

    def step(self, seconds, speed, in_zone, approach_speed, release_allowed=False, valid=True):
        if not self._handle:
            raise RuntimeError('Stop controller is closed')
        out = _Output()
        ok = self._dll.stop_step(self._handle, seconds, speed, int(in_zone), approach_speed,
                                 int(release_allowed), int(valid), ctypes.byref(out))
        if not ok or not 0 <= out.phase < len(PHASES):
            raise RuntimeError('Stop controller ABI call failed')
        return StopResult(out.target, out.held, PHASES[out.phase], bool(out.ready))

    def reset(self, dwell_seconds, exclusive=False):
        if not self._handle or not self._dll.stop_reset(self._handle, dwell_seconds, int(exclusive)):
            raise ValueError('Invalid reset or closed stop controller')

    def close(self):
        if getattr(self, '_handle', None):
            self._dll.stop_destroy(self._handle)
            self._handle = None

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    def __del__(self):
        self.close()


if __name__ == '__main__':
    path, info = build_library()
    print(json.dumps({'library': str(path), **info}, indent=2))
