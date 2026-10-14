import math
import unittest
from tools.competition_steering import SteeringController, carla_to_formal


class SteeringTests(unittest.TestCase):
    def test_straight_path_and_production_lookahead(self):
        with SteeringController() as c:
            out=c.step([(i,0,0) for i in range(101)],10,0,0,3)
            self.assertAlmostEqual(out['command'],0)
            self.assertEqual(out['nearest_index'],10)
            self.assertEqual(out['target_index'],13)

    def test_coordinate_handedness_and_degrees(self):
        pose=carla_to_formal(0,0,0)
        with SteeringController() as c:
            right=c.step([(i,-1,0) for i in range(30)],*pose,3)
            self.assertGreater(right['command'],0)  # CARLA positive steer turns right
        with SteeringController() as c:
            left=c.step([(i,1,0) for i in range(30)],*pose,3)
            self.assertLess(left['command'],0)
        with SteeringController() as c:
            out=c.step([(0,-i,0) for i in range(30)],*carla_to_formal(0,0,90),3)
            self.assertAlmostEqual(out['command'],0,places=5)

    def test_real_pid_history_and_independent_handles(self):
        path=[(i,1,0) for i in range(30)]
        with SteeringController() as a, SteeringController() as b:
            first=a.step(path,0,0,0,3)
            second=a.step(path,0,0,0,3)
            isolated=b.step(path,0,0,0,3)
            self.assertAlmostEqual(first['command'],isolated['command'])
            self.assertAlmostEqual(first['command'],first['raw']*(a.parameters['steerKp']+a.parameters['steerKd']),places=5)
            self.assertAlmostEqual(second['command'],second['raw']*(a.parameters['steerKp']+a.parameters['steerKi']),places=5)

    def test_invalid_state_path_and_lifetime(self):
        c=SteeringController()
        path=[(0,0,0),(10,0,0)]
        for bad in ([],[(0,0,0)],[(0,0,0),(math.nan,0,0)]):
            with self.assertRaises(ValueError): c.step(bad,0,0,0,3)
        for speed in (-1,math.nan,math.inf):
            with self.assertRaises(ValueError): c.step(path,0,0,0,speed)
        c.close(); c.close()
        with self.assertRaises(RuntimeError): c.step(path,0,0,0,3)
        with self.assertRaises(ValueError): carla_to_formal(0,math.inf,0)


if __name__=='__main__': unittest.main()
