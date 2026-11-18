"""Tests unitarios del protocolo de aplicación (protocolos.py).
Ejecutar desde la raíz del repositorio:  python -m unittest discover tests
"""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'interfaz'))
import protocolos as P  # noqa: E402


class TestComandos(unittest.TestCase):
    """Comandos Tierra -> Satélite."""

    def test_periodos(self):
        self.assertEqual(P.cmd_periodo_th(3000), "1:3000")
        self.assertEqual(P.cmd_periodo_dist(150.0), "6:150")

    def test_modo_y_joystick(self):
        self.assertEqual(P.cmd_modo_auto(), "3:")
        self.assertEqual(P.cmd_joystick(-1), "2:-1")

    def test_toggles(self):
        self.assertEqual(P.cmd_toggle_th(False), "4:0")
        self.assertEqual(P.cmd_toggle_th(True), "4:1")
        self.assertEqual(P.cmd_toggle_dist(False), "5:0")
        self.assertEqual(P.cmd_origen_media(True), "8:1")

    def test_angulo(self):
        self.assertEqual(P.cmd_angulo_fijo(90), "7:90")
        self.assertEqual(P.cmd_angulo_fijo(270.4), "7:270")
        self.assertEqual(P.cmd_angulo_fijo(360), "7:0")


class TestTramas(unittest.TestCase):
    """Tramas Satélite -> Tierra."""

    def test_parsear_trama(self):
        self.assertEqual(P.parsear_trama("1:23.5:40.0\r\n"), ("1", ["23.5", "40.0"]))
        self.assertEqual(P.parsear_trama("4:"), ("4", [""]))
        self.assertEqual(P.parsear_trama(""), ("", []))

    def test_th(self):
        self.assertEqual(P.parsear_th(["23.5", "40.0"]), (23.5, 40.0))
        with self.assertRaises(ValueError):
            P.parsear_th(["abc", "40"])

    def test_distancia(self):
        codigo, campos = P.parsear_trama("2:35:-1:120:0")
        self.assertEqual(codigo, P.DISTANCIA)
        self.assertEqual(P.parsear_distancia(campos), (35.0, -1.0, 120.0, P.MODO_RASTREO))

    def test_distancia_parada_y_fijo(self):
        _, campos = P.parsear_trama("2:-2:-2:45:2")
        dF, dT, ang, modo = P.parsear_distancia(campos)
        self.assertEqual((dF, dT, modo), (P.DIST_PARADA, P.DIST_PARADA, P.MODO_FIJO))

    def test_distancia_mal_formada(self):
        with self.assertRaises(ValueError):
            P.parsear_distancia(["35", "120", "0"])          # Formato antiguo
        with self.assertRaises(ValueError):
            P.parsear_distancia(["35", "20", "120", "7"])    # Modo inexistente

    def test_angulo_trasero(self):
        self.assertEqual(P.angulo_trasero(0), 180)
        self.assertEqual(P.angulo_trasero(90), 270)
        self.assertEqual(P.angulo_trasero(180), 0)


if __name__ == '__main__':
    unittest.main()
