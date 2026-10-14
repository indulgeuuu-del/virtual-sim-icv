"""Bridge the exact production pursuit overload and PositionalPID.

Following-mode fixed gains only: no map/update/drive, lane-change offset or
manual-mode curvature gain. CARLA reflects y/yaw into right-handed coordinates.
The returned command is not a verified SimOne-to-CARLA actuator calibration.
"""
import ctypes
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import subprocess

if __package__:
    from .competition_following import read_source, extract
else:
    from competition_following import read_source, extract

ROOT=Path(__file__).resolve().parents[1]
SOURCES=('competition/util/UtilDriver.h', 'competition/TrajectoryControl/src/tools/controller.hpp',
         'tools/native/steering_bridge.cpp', 'tools/competition_steering.py', 'tools/competition_following.py')


def build_library():
    if os.name!='nt' or struct.calcsize('P')!=8:
        raise RuntimeError('Steering bridge requires Windows x64 and MSVC')
    raw={name:(ROOT/name).read_bytes() for name in SOURCES}
    hashes={name:hashlib.sha256(data).hexdigest() for name,data in raw.items()}
    key=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()[:20]
    output=ROOT/'work/steering_controller'/key
    binary,manifest=output/'steering.dll',output/'build.json'
    if binary.exists() and manifest.exists():
        saved=json.loads(manifest.read_text())
        if saved['source_sha256']==hashes and saved['binary_sha256']==hashlib.sha256(binary.read_bytes()).hexdigest():
            return binary,saved
    signature='static double calculateSteering(const SSD::SimPoint3DVector& targetPath, SimOne_Data_Gps* pGps, size_t &carIndex)'
    pursuit=extract(read_source(raw[SOURCES[0]]),signature,'\n};')
    bridge=read_source(raw[SOURCES[2]])
    if bridge.count('// @PRODUCTION_PURSUIT@')!=1:
        raise ValueError('Review steering bridge insertion point')
    output.mkdir(parents=True,exist_ok=True)
    (output/'bridge.cpp').write_bytes(bridge.replace('// @PRODUCTION_PURSUIT@',pursuit).encode('utf-8'))
    (output/'controller.hpp').write_bytes(read_source(raw[SOURCES[1]]).encode('utf-8'))
    vswhere=Path(os.environ.get('ProgramFiles(x86)','C:/Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
    install=subprocess.check_output([str(vswhere),'-latest','-products','*','-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
    if not install:
        raise RuntimeError('Install Visual Studio Desktop development with C++')
    vcvars=Path(install)/'VC/Auxiliary/Build/vcvars64.bat'
    result=subprocess.run(f'call "{vcvars}" >nul && set',shell=True,capture_output=True,check=True)
    env=os.environ.copy()
    for line in result.stdout.decode('mbcs').splitlines():
        name,sep,value=line.partition('=')
        if sep and name and not name.startswith('='): env[name]=value
    compiler=Path(env['VCToolsInstallDir'])/'bin/Hostx64/x64/cl.exe'
    result=subprocess.run([str(compiler),'/nologo','/LD','/EHsc','/std:c++17','/utf-8',
        '/W4','/O2','/MT','bridge.cpp',f'/Fe:{binary}'],cwd=output,env=env,capture_output=True,timeout=120)
    (output/'compile.log').write_bytes(result.stdout+result.stderr)
    if result.returncode:
        raise RuntimeError(f'Steering build failed: {output / "compile.log"}')
    report=dict(source_sha256=hashes,binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
        extracted_sha256=hashlib.sha256(pursuit.encode()).hexdigest(),toolset=env['VCToolsVersion'].strip(),
        abi_version=1,sdk_build_verified=False,scope='production pursuit overload and PositionalPID, fixed following-mode gains')
    manifest.write_text(json.dumps(report,indent=2),encoding='utf-8')
    return binary,report


class _Output(ctypes.Structure):
    _fields_=[('raw',ctypes.c_double),('command',ctypes.c_double),('nearest',ctypes.c_int),('target',ctypes.c_int)]


class SteeringController:
    def __init__(self):
        binary,self.build=build_library()
        params_raw=(ROOT/'competition/TrajectoryControl/param.json').read_bytes()
        params=json.loads(params_raw.decode('utf-8-sig'))
        self.parameters={k:params[k] for k in ('steerKp','steerKi','steerKd')}
        self.parameter_sha256=hashlib.sha256(params_raw).hexdigest()
        self._dll=ctypes.CDLL(str(binary))
        self._dll.steering_abi_version.restype=ctypes.c_int
        if self._dll.steering_abi_version()!=1: raise RuntimeError('Steering ABI mismatch')
        self._dll.steering_create.argtypes=[ctypes.c_float]*3
        self._dll.steering_create.restype=ctypes.c_void_p
        self._dll.steering_destroy.argtypes=[ctypes.c_void_p]
        self._dll.steering_destroy.restype=None
        self._dll.steering_step.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_double),ctypes.c_int]+[ctypes.c_double]*4+[ctypes.POINTER(_Output)]
        self._dll.steering_step.restype=ctypes.c_int
        self._handle=self._dll.steering_create(*self.parameters.values())
        if not self._handle: raise ValueError('Invalid steering gains')

    def step(self,path,x,y,yaw_radians,speed):
        if not getattr(self,'_handle',None): raise RuntimeError('Steering controller closed')
        if not 2<=len(path)<=10000 or any(len(p)!=3 for p in path): raise ValueError('Expected 2..10000 xyz path points')
        data=(ctypes.c_double*(3*len(path)))(*(v for p in path for v in p))
        out=_Output()
        if not self._dll.steering_step(self._handle,data,len(path),x,y,yaw_radians,speed,ctypes.byref(out)):
            raise ValueError('Invalid path, state, or steering result')
        return dict(raw=out.raw,command=out.command,nearest_index=out.nearest,target_index=out.target)

    def close(self):
        if getattr(self,'_handle',None): self._dll.steering_destroy(self._handle); self._handle=None

    def __enter__(self): return self
    def __exit__(self,*_): self.close()
    def __del__(self): self.close()


def carla_to_formal(x,y,yaw_degrees):
    if not all(math.isfinite(v) for v in (x,y,yaw_degrees)): raise ValueError('Invalid CARLA pose')
    return x,-y,-math.radians(yaw_degrees)
