"""Tests unitarios de la cola circular (media de las 10 últimas temperaturas).
Ejecutar desde la raíz del repositorio:  python -m unittest discover tests
"""

import math
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'interfaz'))
from cola_circular import CircularQueue  # noqa: E402


class TestCircularQueue(unittest.TestCase):

    def test_capacidad_invalida(self):
        """Una cola de capacidad 0 o negativa no se puede crear."""
        with self.assertRaises(ValueError):
            CircularQueue(0)
        with self.assertRaises(ValueError):
            CircularQueue(-3)

    def test_vacia(self):
        """La cola vacía tiene longitud 0 y media NaN."""
        q = CircularQueue(10)
        self.assertEqual(len(q), 0)
        self.assertTrue(math.isnan(q.mean()))
        self.assertFalse(q.is_full())

    def test_media_sin_llenar(self):
        """Con menos de N valores la media es la de los que hay."""
        q = CircularQueue(10)
        for v in (20, 22, 24):
            q.push(v)
        self.assertEqual(len(q), 3)
        self.assertAlmostEqual(q.mean(), 22.0)

    def test_sobrescribe_el_mas_antiguo(self):
        """Al superar la capacidad se descartan los valores más antiguos."""
        q = CircularQueue(10)
        for v in range(1, 16):          # 1..15
            q.push(v)
        self.assertTrue(q.is_full())
        self.assertEqual(len(q), 10)
        self.assertEqual(q.to_list(), list(range(6, 16)))
        self.assertAlmostEqual(q.mean(), sum(range(6, 16)) / 10)

    def test_clear(self):
        """clear() deja la cola vacía y reutilizable."""
        q = CircularQueue(3)
        for v in (1, 2, 3, 4):
            q.push(v)
        q.clear()
        self.assertEqual(len(q), 0)
        q.push(7)
        self.assertEqual(q.to_list(), [7])


if __name__ == '__main__':
    unittest.main()
