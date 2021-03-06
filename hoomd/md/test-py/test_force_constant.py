# -*- coding: iso-8859-1 -*-
# Maintainer: joaander

from hoomd import *
from hoomd import deprecated
from hoomd import md
context.initialize()
import unittest
import os

# tests md.force.constant
class force_constant_tests (unittest.TestCase):
    def setUp(self):
        print
        deprecated.init.create_random(N=100, phi_p=0.05);

        context.current.sorter.set_params(grid=8)

    # test to see that se can create a md.force.constant
    def test_create(self):
        md.force.constant(fx=1.0, fy=0.5, fz=0.74);

    # test changing the force
    def test_change_force(self):
        const = md.force.constant(fx=1.0, fy=0.5, fz=0.74);
        const.set_force(fx=1.45, fy=0.25, fz=-0.1);

    # test the initialization checks
    def test_init_checks(self):
        const = md.force.constant(fx=1.0, fy=0.5, fz=0.74);
        const.cpp_force = None;

        self.assertRaises(RuntimeError, const.set_force, fx=1.45, fy=0.25, fz=-0.1);
        self.assertRaises(RuntimeError, const.enable);
        self.assertRaises(RuntimeError, const.disable);

    def tearDown(self):
        context.initialize();


if __name__ == '__main__':
    unittest.main(argv = ['test.py', '-v'])
