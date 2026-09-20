"""Check the CARLA client and optionally observe an existing server."""

import argparse
import json
import platform
from importlib.metadata import version
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--connect', action='store_true')
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=2000)
    parser.add_argument('--output', type=Path, default=Path('work/carla/client_check.json'))
    args = parser.parse_args()
    report = dict(python=platform.python_version(), client_imported=False,
                  server_checked=args.connect, continuous_frames_verified=False,
                  driving_verified=False, competition_verified=False)
    success = False
    try:
        import carla
        report.update(client_imported=True, package_version=version('carla'))
        success = report['package_version'] == '0.9.15'
        if args.connect:
            client = carla.Client(args.host, args.port)
            client.set_timeout(5.0)
            report.update(client_version=client.get_client_version(),
                          server_version=client.get_server_version())
            world = client.get_world()
            report['map'] = world.get_map().name
            report['synchronous_mode'] = world.get_settings().synchronous_mode
            # Observe only: do not load maps, spawn actors, or take frame ownership.
            frames = [world.wait_for_tick(5.0).frame for _ in range(3)]
            report['frames'] = frames
            report['continuous_frames_verified'] = all(b > a for a, b in zip(frames, frames[1:]))
            success = (success and report['continuous_frames_verified']
                       and report['server_version'] == report['client_version'])
    except Exception as exc:
        report['error'] = f'{type(exc).__name__}: {exc}'
        success = False
    report['passed'] = success
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if success else 1


if __name__ == '__main__':
    raise SystemExit(main())
