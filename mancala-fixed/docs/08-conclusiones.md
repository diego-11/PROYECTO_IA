# 08 — Conclusiones

## Limitaciones Encontradas

**Alpha-Beta con pocos movimientos en la raíz:** Kalah(6,4) tiene máximo 6 movimientos legales en la raíz, lo que limita el paralelismo de root parallelism. Con T=8 hilos y solo 3–6 subárboles, varios hilos quedan ociosos. Estrategias como YBWC o PVS explotan más paralelismo interno, pero son más complejas de implementar correctamente con OpenMP.

**MCTS: speedup sublineal:** dividir las simulaciones entre hilos reduce la profundidad de cada árbol individual. El árbol de MCTS converge más lentamente cuando cada hilo trabaja con la mitad del presupuesto. Root parallelism mitiga esto duplicando la exploración, pero a costa de trabajo redundante en posiciones similares.

**Motor de vida única:** el servidor HTTP del motor es de un solo hilo por petición. Peticiones concurrentes al mismo pod del motor se serializan. Para mayor concurrencia se necesitaría un servidor multi-threaded (como Crow o Boost.Beast con thread pool), o simplemente escalar el número de pods del motor.

**Función de evaluación simple:** la heurística $h = (k_0 - k_1) + \alpha(s_0 - s_1)$ es minimalista. Funciones más sofisticadas (movilidad, captura anticipada, semillas en hoyos 1–3 vs 4–6) mejorarían la calidad de juego de Alpha-Beta, especialmente a profundidades bajas.

## Retos y Cómo Se Resolvieron

**Serialización del tablero:** el índice canónico de 14 enteros (0–5 para P0, 6 para K0, 7–12 para P1, 13 para K1) requirió cuidado en el frontend (que muestra P1 en orden inverso) y en la función `oppPit`. Se resolvió con una suite de tests exhaustivos que verifican cada caso borde.

**CORS en desarrollo:** el frontend en `:8080` haciendo fetch al backend en `:8000` activa la Same-Origin Policy. Se resolvió configurando `CORSMiddleware` con orígenes explícitos y manejando correctamente el preflight `OPTIONS`.

**Compatibilidad de OpenMP:** en Debian/Ubuntu el flag `-fopenmp` requiere `libomp-dev`. El Dockerfile instala explícitamente `libomp5` en la imagen runtime (no solo en el builder) para que `libgomp.so` esté disponible al ejecutar el binario.

**Tests del backend sin el motor real:** los tests de `backend/tests/test_api.py` usan `unittest.mock.AsyncMock` para simular el cliente HTTP, lo que permite verificar la lógica de validación y manejo de errores sin necesitar el contenedor del motor ejecutándose.

## Lecciones Aprendidas

El contraste entre Alpha-Beta y MCTS como sujetos de paralelización ilustra perfectamente la distinción entre **paralelismo con dependencias** (poda Alpha-Beta) y **paralelismo embarrassingly parallel** (simulaciones MCTS). MCTS es conceptualmente más fácil de paralelizar, pero el beneficio real depende de si el presupuesto de simulaciones o el tiempo es el parámetro de control.

La separación estricta de contenedores (motor, backend, frontend) forzó a diseñar una API HTTP bien definida entre el motor C++ y el backend Python. Esto resultó en un sistema más robusto: cada componente puede actualizarse, escalarse o remplazarse de forma independiente, lo que es exactamente el propósito de la arquitectura de microservicios.

Kubernetes demuestra su valor no solo en el escalado horizontal, sino en las capacidades de auto-recuperación: si un pod del backend cae, el Service redirige automáticamente las peticiones a las réplicas sanas gracias a los readiness probes.

## Recomendaciones de Mejoras Futuras

- **YBWC para Alpha-Beta:** implementar Young Brothers Wait Concept reduciría la pérdida de podas en root parallelism: el primer hijo se explora secuencialmente para obtener una buena cota β, y los hermanos restantes se paralelizan.
- **Tree Parallelization para MCTS:** compartir un único árbol con `omp atomic` en los contadores y *virtual loss* para evitar que múltiples hilos sigan la misma rama simultáneamente.
- **Iterative Deepening:** combinar Alpha-Beta con profundización iterativa para que el motor use el tiempo disponible de forma más uniforme.
- **Horizontal Pod Autoscaler (HPA):** configurar el HPA de Kubernetes para escalar automáticamente el número de réplicas del backend en función de la latencia media o el uso de CPU.
- **Función de evaluación aprendida:** entrenar una red neuronal pequeña sobre partidas de Alpha-Beta a profundidad alta y usarla como función de evaluación para MCTS, combinando lo mejor de los dos paradigmas (AlphaZero-style).
