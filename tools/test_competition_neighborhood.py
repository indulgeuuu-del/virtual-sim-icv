"""Test extracted production neighborhood logic and audit its call order; no SDK."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


def read_source(path):
    raw = path.read_bytes()
    try:
        return raw.decode('utf-8-sig')
    except UnicodeDecodeError:
        return raw.decode('gb18030')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    manifest = json.loads((root/'competition/baseline.json').read_text(encoding='utf-8-sig'))
    base = root/(manifest['source_root'] if args.baseline else 'competition')/'TrajectoryControl/src'
    output = root/'work/competition_neighborhood'/('baseline' if args.baseline else 'development')
    output.mkdir(parents=True, exist_ok=True)
    paths = [base/'core/vehicle.cpp', base/'core/vehicle.h', base/'main_track.cpp']
    report = {'source_sha256': {p.relative_to(base).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in paths}, 'sdk_build_verified': False, 'tests': {}}
    try:
        cpp, header, track = map(read_source, paths)
        update = cpp.split('bool MainVehicle::update(', 1)[1].split('// 更新主车控制参数', 1)[0]
        legacy = 'void MainVehicle::rebuildNeighborhood(void)' not in cpp
        if legacy:
            block = cpp[cpp.index('\tstatic constexpr float OFFSET'):cpp.index('\n\treturn FlagType::isMainVehicleInitialized;')]
            rebuild = 'void MainVehicle::rebuildNeighborhood(void) {\n'+block+'\n}\n'
            assert track.index('mainVehicle.update();') < track.index('obstacleList.clear();')
            assert 'rebuildNeighborhood' not in track
            report['call_order_passed'] = False
        else:
            rebuild = cpp.split('void MainVehicle::rebuildNeighborhood(void)', 1)[1].split('// 更新主车控制参数', 1)[0]
            rebuild = 'void MainVehicle::rebuildNeighborhood(void)'+rebuild
            update_only = re.sub(r'//[^\n]*', '', update.split('void MainVehicle::rebuildNeighborhood', 1)[0])
            assert 'neighborhood.clear();' in update_only and 'obstacleList' not in update_only
            assert track.count('mainVehicle.rebuildNeighborhood();') == 1
            call = track.index('mainVehicle.rebuildNeighborhood();')
            assert track.index('mainVehicle.update();') < track.index('obstacleList.clear();')
            assert track.rindex('obstacleList.push_back(') < call < track.index('mainVehicle.neighborhood.')
            assert not re.search(r'obstacleList\.(clear|push_back|erase|insert|assign|resize|swap)\(', track[call:])
            assert 'void rebuildNeighborhood(void);' in header
            report['call_order_passed'] = True
        # Keep production Neighborhood declarations, geometry constants, classification,
        # sort and query bodies. Only SDK-dependent vehicle/obstacle types are replaced.
        declarations = header[header.index('#define MAIN_VEHICLE_WIDTH'):header.index('class MainVehicle')]
        methods = cpp[cpp.index('void Neighborhood::clear(void)'):]
        harness = '''#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
struct Obstacle { int id, laneID; double sRelativeToVehicle, tRelativeToVehicle; float velocityPlanar; };
struct CheckedList : std::vector<Obstacle> {
    Obstacle& operator[](size_t i) { return at(i); }
};
CheckedList obstacleList;
bool isSameRoadId(int a, int b) { return a==b; }
'''+declarations+'''
class MainVehicle {
public:
    double s=0, t=0;
    int laneID=1;
    Neighborhood neighborhood;
    void rebuildNeighborhood(void);
};
MainVehicle mainVehicle;
'''+f'constexpr bool legacyOrder = {str(legacy).lower()};\n'+rebuild+methods
        harness += read_source(root/'tests/cpp/neighborhood_test.cpp')
        (output/'test.cpp').write_text(harness, encoding='utf-8')
        if os.name != 'nt':
            raise RuntimeError('Windows/MSVC required')
        vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
        install = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
                   'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        if not install:
            raise RuntimeError('Install Desktop development with C++')
        vcvars = Path(install)/'VC/Auxiliary/Build/vcvars64.bat'
        result = subprocess.run(f'call "{vcvars}" >nul && set', shell=True, capture_output=True, check=True)
        env = os.environ.copy()
        for line in result.stdout.decode('mbcs').splitlines():
            key, sep, value = line.partition('=')
            if sep and key and not key.startswith('='):
                env[key] = value
        compiler = Path(env['VCToolsInstallDir'])/'bin/Hostx64/x64/cl.exe'
        result = subprocess.run([str(compiler), '/nologo', '/EHsc', '/std:c++17', '/utf-8', '/W4',
                                 'test.cpp', '/Fe:test.exe'], cwd=output, env=env, capture_output=True)
        (output/'compile.log').write_bytes(result.stdout+result.stderr)
        if result.returncode:
            raise RuntimeError(f'Compile failed: {output / "compile.log"}')
        for name in ('first_frame','disappear','shrink','reorder','speed_change','region_change',
                     'all_regions','stationary_and_reset','boundaries'):
            result = subprocess.run([str(output/'test.exe'), name], cwd=output, env=env,
                                    capture_output=True, text=True, timeout=10)
            report['tests'][name] = {'passed': result.returncode==0, 'output': (result.stdout+result.stderr).strip()}
        report['passed'] = report['call_order_passed'] and all(v['passed'] for v in report['tests'].values())
    except (AssertionError, OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        report.update(passed=False, error=f'{type(error).__name__}: {error}')
    (output/'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
