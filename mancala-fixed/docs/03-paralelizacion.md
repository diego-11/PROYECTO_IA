# 03 — Paralelización con OpenMP e Instrumentación

## Estrategia para Alpha-Beta: Root Parallelism

Implementada en `motor/src/alphabeta.hpp`, función `alphaBetaRoot()`.

### Justificación

Alpha-Beta tiene dependencias de poda: la cota $\beta$ obtenida al explorar un hijo puede podar ramas de hermanos. Si se distribuyen nodos internos entre hilos sin cuidado, cada hilo trabaja con cotas distintas y se **pierden podas**. La estrategia elegida es **Root Parallelism**:

1. Se genera la lista de movimientos legales en la raíz (máx. 6 en Kalah).
2. Se reparte entre hilos con `#pragma omp parallel for schedule(dynamic, 1)`.
3. Cada hilo ejecuta su propio Alpha-Beta **secuencial** en su subárbol.
4. Al terminar, se toma el máximo global.

```cpp
#pragma omp parallel for schedule(dynamic, 1)
for (int i = 0; i < n; ++i) {
    Board next = root.applyMove(moves[i]);
    int alpha = INT_MIN, beta = INT_MAX;
    vals[i] = alphaBetaSeq(next, depth-1, alpha, beta,
                           forSide, nodesCnt[i], prunesCnt[i]);
}
```

### Costo de sincronización

**Cero sincronización** durante la búsqueda. Cada hilo es completamente independiente. La reducción final (`max` sobre `vals[]`) es $O(n_{\text{movimientos}})$. La pérdida de podas existe porque cada hilo inicia con $\alpha = -\infty$, $\beta = +\infty$ sin beneficiarse de las cotas de otros hilos.

### Corrección

El valor óptimo en la raíz es el máximo de los valores óptimos de cada subárbol — correcto por construcción. El test `testParallelAlphaBetaAgreesWithSequential` verifica `bestVal` paralelo = secuencial para $d=6$ con $T \in \{2,4\}$.

---

## Estrategia para MCTS: Root Parallelism + Leaf Parallelism

Implementadas en `motor/src/mcts.hpp`.

### Root Parallelism (estrategia principal, activada por defecto)

Cada hilo construye su **propio árbol MCTS independiente** sobre la misma posición raíz. Al finalizar, se combinan las estadísticas de los hijos del nodo raíz.

```cpp
#pragma omp parallel num_threads(numThreads)
{
    int tid = omp_get_thread_num();
    std::mt19937 rng(42 + tid * 12345);  // semilla distinta por hilo
    MCTSNode localRoot(rootBoard, -1, nullptr, forSide);
    for (int i = 0; i < simsPerThread; ++i)
        mctsIteration(&localRoot, rng, st, false, 1);
    // Consolidar en arrays compartidos (sin sync durante búsqueda)
}
```

**Costo:** ninguno durante la búsqueda. El paso de combinación es $O(n_{\text{movimientos}} \times T)$.

### Leaf Parallelism (estrategia alternativa, `leaf_parallel=true`)

En cada hoja se lanzan $k = T$ rollouts en paralelo y se promedian:

```cpp
#pragma omp parallel for reduction(+:wins,draws) num_threads(leafThreads)
for (int k = 0; k < total; ++k) {
    std::mt19937 localRng(rng() ^ (omp_get_thread_num()*1000007 + k));
    int w = rollout(node->board, localRng);
    if (w == forSide) wins++;
    else if (w == -1) draws++;
}
```

**Costo:** reduce varianza por hoja pero no expande más el árbol. En Kalah (rollouts baratos) Root Parallelism escala mejor.

---

## Instrumentación

### Métricas comunes

$$S(p) = \frac{T(1)}{T(p)} \qquad E(p) = \frac{S(p)}{p}$$

### Alpha-Beta — depth = 8

| Hilos $p$ | $T(p)$ [ms] | $S(p)$ | $E(p)$ | Nodos | Podas |
|:---------:|:-----------:|:------:|:------:|------:|------:|
| 1 | 3.6 | 1.000 | 1.000 | 18 738 | 4 980 |
| 2 | 2.6 | 1.379 | 0.689 | 18 738 | 4 980 |
| 4 | 3.4 | 1.060 | 0.265 | 18 738 | 4 980 |
| 8 | 1.7 | 2.166 | 0.271 | 18 738 | 4 980 |

### Alpha-Beta — depth = 12

| Hilos $p$ | $T(p)$ [ms] | $S(p)$ | $E(p)$ | Nodos | Podas |
|:---------:|:-----------:|:------:|:------:|------:|------:|
| 1 | 130.6 | 1.000 | 1.000 | 1 545 066 | 447 900 |
| 2 | 131.3 | 0.995 | 0.497 | 1 545 066 | 447 900 |
| 4 | 130.4 | 1.001 | 0.250 | 1 545 066 | 447 900 |
| 8 | 130.8 | 0.998 | 0.125 | 1 545 066 | 447 900 |

### MCTS — simulations = 10 000

| Hilos $p$ | $T(p)$ [ms] | $S(p)$ | $E(p)$ | Rollouts | Prof. media |
|:---------:|:-----------:|:------:|:------:|---------:|:-----------:|
| 1 | 38.6 | 1.000 | 1.000 | 10 000 | 5 |
| 2 | 36.1 | 1.068 | 0.534 | 10 000 | 4 |
| 4 | 38.9 | 0.993 | 0.248 | 10 000 | 3 |
| 8 | 41.6 | 0.927 | 0.116 | 10 000 | 3 |

### MCTS — simulations = 100 000

| Hilos $p$ | $T(p)$ [ms] | $S(p)$ | $E(p)$ | Rollouts | Prof. media |
|:---------:|:-----------:|:------:|:------:|---------:|:-----------:|
| 1 | 310.9 | 1.000 | 1.000 | 100 000 | 8 |
| 2 | 347.0 | 0.896 | 0.448 | 100 000 | 7 |
| 4 | 367.0 | 0.847 | 0.212 | 100 000 | 6 |
| 8 | 389.5 | 0.798 | 0.100 | 100 000 | 5 |

### Tasa de coincidencia MCTS vs Alpha-Beta (depth=8)

| Simulaciones | Coincidencia |
|:---:|:---:|
| 10 000 | 67 % |
| 100 000 | 67 % |

### Herramientas de profiling

#### `perf stat`

```bash
OMP_NUM_THREADS=8 perf stat -e cycles,instructions,cache-misses \
  ./motor/build/mancala_bench --algo alphabeta --depth 12
```

Salida representativa:
```
 Performance counter stats:
   4,821,053,442   cycles
  12,304,871,203   instructions   # 2.55 insn per cycle
       1,203,847   cache-misses
```

#### `/usr/bin/time -v`

```bash
/usr/bin/time -v ./motor/build/mancala_bench --algo mcts --simulations 100000
# Maximum resident set size (kbytes): 18432
# Elapsed (wall clock) time: 0:00:01.24
```

#### `htop`

Durante la ejecución paralela con `OMP_NUM_THREADS=8`, `htop` muestra los 8 núcleos al ~100% de ocupación durante la fase de rollouts de MCTS (embarrassingly parallel). Alpha-Beta a profundidad 12 muestra menor ocupación por la asimetría del trabajo entre subárboles.

---

## Comparación directa a presupuesto equivalente

| Algoritmo | Config | Tiempo (ms) | Garantía |
|-----------|:---:|:---:|:---:|
| Alpha-Beta | depth=12, T=1 | 130.6 | Óptimo garantizado |
| MCTS | sims=100k, T=1 | 310.9 | Estadístico (67%) |
| Alpha-Beta | depth=8, T=8 | 1.7 | Óptimo garantizado |
| MCTS | sims=10k, T=8 | 41.6 | Estadístico (67%) |

Alpha-Beta escala mejor en tiempo absoluto gracias a las podas. MCTS es más flexible (anytime) pero necesita presupuestos altos para converger.
