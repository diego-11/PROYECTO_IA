# Mancala Kalah — Documentación del Proyecto

**Proyecto Final · Infraestructuras Paralelas y Distribuidas + Inteligencia Artificial**  
Universidad del Valle — Junio 2026

---

## Índice

| # | Archivo | Contenido |
|---|---------|-----------|
| 01 | [01-arquitectura.md](01-arquitectura.md) | Componentes, diagrama Mermaid, API REST, CORS |
| 02 | [02-motor.md](02-motor.md) | Reglas Kalah(6,4), Minimax+Alpha-Beta, MCTS+UCT, pruebas |
| 03 | [03-paralelizacion.md](03-paralelizacion.md) | Estrategia OpenMP, instrumentación, tablas T/S/E, gráficas |
| 04 | [04-despliegue-local.md](04-despliegue-local.md) | Docker Compose + Kubernetes local (kind/minikube) |
| 05 | [05-despliegue-nube.md](05-despliegue-nube.md) | Manifiestos YAML cloud, evidencias kubectl |
| 06 | [06-cicd.md](06-cicd.md) | GitHub Actions + SonarQube declarado en YAML |
| 07 | [07-analisis-comparativo.md](07-analisis-comparativo.md) | Local vs. nube: p50/p95, throughput, escalado |
| 08 | [08-conclusiones.md](08-conclusiones.md) | Limitaciones, retos, trabajo futuro, lecciones |

---

## Mapeo criterio → archivo

| Criterio de la rúbrica | Archivo donde se evalúa |
|------------------------|------------------------|
| Motores de Mancala: corrección | [02-motor.md](02-motor.md) |
| Paralelización con OpenMP | [03-paralelizacion.md](03-paralelizacion.md) |
| Instrumentación local | [03-paralelizacion.md](03-paralelizacion.md) |
| Separación de componentes | [01-arquitectura.md](01-arquitectura.md) |
| Despliegue local | [04-despliegue-local.md](04-despliegue-local.md) |
| Despliegue en la nube con Kubernetes | [05-despliegue-nube.md](05-despliegue-nube.md) |
| CI/CD y calidad de código | [06-cicd.md](06-cicd.md) |
| Análisis comparativo local vs. nube | [07-analisis-comparativo.md](07-analisis-comparativo.md) |
| Claridad de explicaciones | Transversal a todos los archivos |
| Conclusiones | [08-conclusiones.md](08-conclusiones.md) |

---

## Inicio rápido

```bash
git clone <repo-url>
cd mancala
docker compose -f deploy/local/docker-compose.yml up --build
# Frontend: http://localhost:8080
```
