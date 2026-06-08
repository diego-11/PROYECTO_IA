# Mancala Kalah — Motores Paralelos y Despliegue en Kubernetes

**Proyecto Final — Infraestructuras Paralelas y Distribuidas / Inteligencia Artificial**  
Universidad del Valle, Sede Tuluá · Junio 2026  
Profesor: Carlos Andres Delgado S, Msc

---

## Integrantes del Grupo

| **Nombres** | Codigo |
|-------|-------------|
| Juan David Agudelo | 2359519 |
| Daniel Alexander Ramirez Gelviz | 2659652 |
| David Stiven Mujanajinsoy Jajoy |2376834 |
| Diego Andrés Bolaños Isiquita |2379918 |
| Jesus Alberto Tunubala | 2379924  |

> **IMPORTANTE:** Reemplaza los nombres, códigos y correos con los datos reales del grupo antes de hacer push.

---

## Descripción

Motor de juego para **Mancala Kalah(6,4)** con dos algoritmos de búsqueda:

- **Alpha-Beta** (Minimax con poda, Root Parallelism con OpenMP)
- **MCTS con UCT** (búsqueda estocástica, Root/Leaf Parallelism con OpenMP)

Expuesto como API REST (FastAPI) y orquestado con **Kubernetes** (local con kind/minikube, nube con GKE/EKS/AKS).

---

## Inicio Rápido (Docker Compose — recomendado)

```bash
# 1. Clonar el repositorio
git clone <repo-url>
cd mancala

# 2. Levantar toda la aplicación (un solo comando)
docker compose -f deploy/local/docker-compose.yml up --build

# 3. Abrir en el navegador
#    Frontend:  http://localhost:8080
#    API docs:  http://localhost:8000/docs
#    Salud:     http://localhost:8000/healthz
```

---

## Construcción del Motor C++ (sin Docker)

```bash
cd motor
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Ejecutar tests
./build/mancala_tests

# Benchmarks
OMP_NUM_THREADS=4 ./build/mancala_bench --algo alphabeta --depth 8
OMP_NUM_THREADS=4 ./build/mancala_bench --algo mcts --simulations 10000
```

---

## Tests del Backend Python

```bash
cd backend
pip install -r requirements.txt pytest pytest-asyncio anyio
python -m pytest tests/ -v
```

---

## Kubernetes Local (kind)

```bash
kind create cluster --name mancala
kubectl apply -f deploy/local/k8s/namespace.yaml
kubectl apply -f deploy/local/k8s/configmap.yaml
kubectl apply -f deploy/local/k8s/motor.yaml
kubectl apply -f deploy/local/k8s/backend.yaml
kubectl apply -f deploy/local/k8s/frontend.yaml
kubectl get pods,svc -n mancala
```

---

## Estructura del Repositorio

```
.
├── README.md
├── docs/                    # Informe (8 archivos Markdown obligatorios)
├── motor/                   # Motor C++/OpenMP
│   ├── src/                 # board.hpp, alphabeta.hpp, mcts.hpp, server.cpp
│   ├── tests/               # test_main.cpp (12 tests)
│   ├── bench/               # benchmark.cpp + suite.txt
│   └── Dockerfile
├── backend/                 # Wrapper FastAPI Python
│   ├── app/main.py
│   ├── tests/test_api.py
│   └── Dockerfile
├── frontend/                # Cliente web nginx
│   ├── html/index.html
│   ├── nginx.conf
│   └── Dockerfile
├── deploy/
│   ├── local/               # docker-compose.yml + manifiestos k8s local
│   └── cloud/               # manifiestos YAML para GKE/EKS/AKS
└── .github/workflows/       # CI/CD + SonarQube (YAML)
```

---

## Informe

Ver [`docs/README.md`](docs/README.md) para el índice completo y el mapeo a la rúbrica.
