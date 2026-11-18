"""Cola circular de capacidad fija (para la media móvil de las últimas N temperaturas)."""


class CircularQueue:
    """Cola circular: cuando está llena, push() sobrescribe el valor más antiguo."""

    def __init__(self, capacidad):
        """Crea la cola vacía.
        Parámetros: capacidad (int > 0). Resultado: ninguno.
        Lanza ValueError si la capacidad no es positiva."""
        if capacidad <= 0:
            raise ValueError("La capacidad debe ser mayor que 0")
        self.capacidad = capacidad
        self._datos = [0.0] * capacidad
        self._cabeza = 0      # Posición donde se escribirá el siguiente valor
        self._n = 0           # Valores guardados (máximo = capacidad)

    def push(self, valor):
        """Añade un valor; si la cola está llena sustituye al más antiguo.
        Parámetros: valor (float). Resultado: ninguno."""
        self._datos[self._cabeza] = valor
        self._cabeza = (self._cabeza + 1) % self.capacidad
        if self._n < self.capacidad:
            self._n += 1

    def mean(self):
        """Media de los valores guardados.
        Parámetros: ninguno. Resultado: float (NaN si la cola está vacía)."""
        if self._n == 0:
            return float('nan')
        return sum(self.to_list()) / self._n

    def to_list(self):
        """Valores guardados, del más antiguo al más reciente.
        Parámetros: ninguno. Resultado: list."""
        inicio = (self._cabeza - self._n) % self.capacidad
        return [self._datos[(inicio + i) % self.capacidad] for i in range(self._n)]

    def is_full(self):
        """Indica si la cola está llena.
        Parámetros: ninguno. Resultado: bool."""
        return self._n == self.capacidad

    def clear(self):
        """Vacía la cola. Parámetros: ninguno. Resultado: ninguno."""
        self._cabeza = 0
        self._n = 0

    def __len__(self):
        """Número de valores guardados. Resultado: int."""
        return self._n
