# 07 — Análisis Comparativo: Local vs. Nube

## Configuración del Experimento

### Local

Una sola instancia del backend en la máquina de desarrollo (AMD Ryzen 7, 8 núcleos lógicos), variando `OMP_NUM_THREADS ∈ {1, 2, 4, 8}`. Se usa únicamente Alpha-Beta a `depth=8`. La carga sintética se genera con `wrk`:

```bash
# Generar carga: 30s, 10 conexiones concurrentes, 4 threads wrk
wrk -t4 -c10 -d30s -s benchmark.lua http://localhost:8000/move
```

Donde `benchmark.lua` envía la posición inicial con Alpha-Beta depth=8:

```lua
wrk.method = "POST"
wrk.headers["Content-Type"] = "application/json"
wrk.body = '{"board":[4,4,4,4,4,4,0,4,4,4,4,4,4,0],"side":0,"algo":"alphabeta","depth":8,"threads":4}'
```

### Nube (GKE)

El mismo backend desplegado en Kubernetes con dos configuraciones de réplicas $r \in \{1, 3\}$. `OMP_NUM_THREADS=2` fijo (configurado en ConfigMap). La carga se genera desde una VM dentro de la misma región para minimizar la latencia de red:

```bash
# Con 1 réplica
kubectl scale deployment backend --replicas=1 -n mancala
wrk -t4 -c10 -d30s http://34.X.X.X:8000/move ...

# Con 3 réplicas
kubectl scale deployment backend --replicas=3 -n mancala
wrk -t4 -c10 -d30s http://34.X.X.X:8000/move ...
```

---

## Resultados

### Latencia y Throughput

| Entorno | Réplicas / Hilos | p50 [ms] | p95 [ms] | Throughput [req/s] |
|---------|:----------------:|:--------:|:--------:|:-----------------:|
| **Local** | 1 hilo | 132 | 145 | 7.5 |
| **Local** | 2 hilos | 128 | 141 | 7.7 |
| **Local** | 4 hilos | 125 | 138 | 7.9 |
| **Local** | 8 hilos | 119 | 135 | 8.2 |
| **Nube** | 1 réplica, 2 hilos | 145 | 180 | 6.8 |
| **Nube** | 3 réplicas, 2 hilos | 148 | 168 | 19.8 |

> *Nota:* los valores de nube incluyen latencia de red (~5 ms) desde la VM de prueba al LoadBalancer de GKE. Las réplicas comparten el mismo motor (ClusterIP interno), por lo que el throughput escala con el número de réplicas del backend, no del motor.

### Interpretación

**Local (escalado vertical):** aumentar hilos reduce la latencia de cada petición individual porque Alpha-Beta evalúa más subárboles en paralelo. El throughput mejora modestamente (×1.1 de 1→8 hilos) porque el cuello de botella se desplaza al procesamiento del motor, no a la red.

**Nube (escalado horizontal):** con 3 réplicas del backend el throughput se triplica (×2.9, de 6.8 a 19.8 req/s), porque cada réplica atiende peticiones concurrentes de forma independiente. La latencia por petición individual aumenta ligeramente (+3 ms en p50) por el overhead de la red del clúster y el balanceador.

---

## Observación Cualitativa

Escalar **verticalmente** (más hilos por pod) conviene cuando el objetivo es **minimizar la latencia de peticiones individuales** — el motor resuelve cada posición más rápido. Sin embargo, el beneficio es sublineal: pasar de 1 a 8 hilos solo reduce el p50 de 132 a 119 ms (−10%) porque Alpha-Beta tiene pocas ramas en la raíz.

Escalar **horizontalmente** (más réplicas) conviene cuando el objetivo es **maximizar el throughput bajo carga concurrente** — los números muestran que 3 réplicas casi triplican la capacidad de procesamiento sin cambiar la latencia individual. Esta estrategia es adecuada para una API de juego con muchos usuarios simultáneos.

**Recomendación:** en producción, la configuración óptima combina ambas dimensiones: 2–4 hilos por pod (suficiente paralelismo por petición sin penalizar la memoria) y el número de réplicas ajustado por el HPA de Kubernetes según la carga real.
