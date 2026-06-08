# 02 — Motor: Reglas, Algoritmos y Pruebas

## Reglas de Kalah(6,4)

Implementadas en `motor/src/board.hpp`. La variante estándar exige:

- **Tablero:** 2 filas × 6 hoyos + 2 kalahas (almacenes). Representación como arreglo de 14 enteros.
- **Semillas iniciales:** 4 por hoyo, 0 en cada kalaha.
- **Siembra:** el jugador toma todas las semillas de un hoyo propio y las distribuye una a una en sentido antihorario, incluyendo su propio kalaha pero **saltando el del oponente**.
- **Turno extra:** si la última semilla cae en el kalaha propio, el jugador repite turno.
- **Captura:** si la última semilla cae en un **hoyo propio vacío** y el hoyo **opuesto del rival** contiene semillas, el jugador captura ambos grupos hacia su kalaha.
- **Fin:** cuando uno de los lados queda completamente vacío; el otro jugador mueve todas sus semillas restantes a su kalaha. Gana quien tenga más semillas.

### Índices canónicos

```
  P1 store (13) ← [12][11][10][ 9][ 8][ 7] ← P1 pits
                   [ 0][ 1][ 2][ 3][ 4][ 5] → P0 pits → P0 store (6)
```

La función `oppPit(i)` mapea:
- Hoyo `i` de P0 (0–5) → hoyo opuesto P1: `12 - i`
- Hoyo `i` de P1 (7–12) → hoyo opuesto P0: `19 - i`

## Motor 1: Minimax con Poda Alfa-Beta

### Función de evaluación heurística

$$h(\text{estado}) = (k_{\text{propio}} - k_{\text{rival}}) + \alpha \cdot (s_{\text{propio}} - s_{\text{rival}})$$

donde $k$ son las semillas en el kalaha, $s$ las semillas en los hoyos del lado correspondiente, y $\alpha = 0.1 \in [0,1]$.

### Pseudocódigo — Minimax puro (referencia de corrección)

```
function MINIMAX(estado, profundidad, jugador_max):
    si profundidad = 0 o estado.terminal():
        retornar h(estado)
    si estado.turno = jugador_max:
        mejor ← -∞
        para cada movimiento en estado.movimientos_legales():
            valor ← MINIMAX(estado.aplicar(movimiento), profundidad-1, jugador_max)
            mejor ← max(mejor, valor)
        retornar mejor
    sino:
        mejor ← +∞
        para cada movimiento en estado.movimientos_legales():
            valor ← MINIMAX(estado.aplicar(movimiento), profundidad-1, jugador_max)
            mejor ← min(mejor, valor)
        retornar mejor
```

### Pseudocódigo — Alpha-Beta

```
function ALPHA_BETA(estado, prof, α, β, jugador_max):
    si prof = 0 o estado.terminal():
        retornar h(estado)
    si estado.turno = jugador_max:
        mejor ← -∞
        para cada movimiento en estado.movimientos_legales():
            valor ← ALPHA_BETA(estado.aplicar(movimiento), prof-1, α, β, jugador_max)
            mejor ← max(mejor, valor)
            α     ← max(α, mejor)
            si β ≤ α: romper  // poda Beta
        retornar mejor
    sino:
        mejor ← +∞
        para cada movimiento en estado.movimientos_legales():
            valor ← ALPHA_BETA(estado.aplicar(movimiento), prof-1, α, β, jugador_max)
            mejor ← min(mejor, valor)
            β     ← min(β, mejor)
            si β ≤ α: romper  // poda Alfa
        retornar mejor
```

**Corrección:** Alpha-Beta con la misma profundidad produce el mismo movimiento óptimo que Minimax puro (evidencia: test `testAlphaBetaEqualsMinimaxSmallDepth` en `motor/tests/test_main.cpp`, verificado para $d \in \{1,2,3,4,5\}$).

## Motor 2: MCTS con Política UCT

### Política de selección UCT

$$\text{UCT}(n) = \frac{w_n}{N_n} + c \cdot \sqrt{\frac{\ln N_{\text{padre}}}{N_n}}, \quad c = \sqrt{2}$$

donde $w_n$ son las victorias acumuladas, $N_n$ el número de visitas al nodo $n$ y $N_{\text{padre}}$ las visitas del nodo padre.

### Pseudocódigo — Ciclo MCTS

```
función MCTS(raíz, presupuesto):
    para i = 1 hasta presupuesto:
        // 1. Selección
        nodo ← raíz
        mientras nodo no es hoja y está completamente expandido:
            nodo ← argmax_{hijo} UCT(hijo)

        // 2. Expansión
        si nodo no está completamente expandido:
            movimiento ← elegir_movimiento_no_probado(nodo)
            nodo ← crear_hijo(nodo, movimiento)

        // 3. Simulación (rollout aleatorio)
        resultado ← JUGAR_ALEATORIAMENTE_HASTA_FIN(nodo.estado)

        // 4. Retropropagación
        mientras nodo ≠ null:
            nodo.visitas += 1
            si resultado = jugador_raíz: nodo.victorias += 1
            si resultado = empate:       nodo.victorias += 0.5
            nodo ← nodo.padre

    retornar hijo_más_visitado(raíz)
```

### Corrección estadística

MCTS no garantiza el movimiento óptimo; su corrección es estadística. La **tasa de coincidencia** con Alpha-Beta (profundidad 8) sobre las 3 posiciones de prueba:

| Simulaciones | Coincidencia |
|:---:|:---:|
| 1 000 | ~33% |
| 10 000 | ~67% |
| 100 000 | ~67% |

La convergencia es gradual y depende de la posición. En la posición inicial (más estudiada), MCTS converge al mismo movimiento que Alpha-Beta con presupuestos altos.

## Suite de Pruebas Unitarias

Archivo: `motor/tests/test_main.cpp`. Ejecutar con:

```bash
cd motor/build
./mancala_tests
```

| Test | Qué verifica |
|------|-------------|
| `testInitialBoard` | Estado inicial correcto (4 semillas por hoyo, 0 en kalahas) |
| `testLegalMoves` | Devuelve exactamente los 6 hoyos propios con semillas |
| `testSowing` | Siembra correcta: pit 0 con 4 semillas → distribución en 1,2,3,4 |
| `testExtraTurn` | Última semilla en kalaha propio → `side` no cambia |
| `testCapture` | Captura correcta (sin falsos positivos) |
| `testTerminal` | Detección de fin de juego y ganador |
| `testEvaluation` | Heurística: kalaha mayor → evaluación positiva |
| `testOppPit` | Mapeo de hoyos opuestos correcto |
| `testAlphaBetaEqualsMinimaxSmallDepth` | Alpha-Beta = Minimax para $d \in \{1..5\}$ |
| `testMCTSConverges` | MCTS retorna movimiento válido con winRate $\in [0,1]$ |
| `testParallelAlphaBetaAgreesWithSequential` | Paralelo (T=2,4) = secuencial para $d=6$ |
| `testMCTSParallelRootAndLeaf` | Root y Leaf parallelism retornan movimientos válidos |

Todos los 12 tests pasan (`=== All tests passed ===`).
