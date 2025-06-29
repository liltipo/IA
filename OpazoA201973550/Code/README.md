# VRPB Solver - Greedy + Hill Climbing

Este proyecto resuelve el **Vehicle Routing Problem with Backhauls (VRPB)** usando una heurística **Greedy** para generar una solución inicial y luego la mejora con **Hill Climbing Best Improvement (HC-BI)**.

---

## 📁 Estructura del Proyecto

- `src/`: Contiene los archivos fuente `v1.cpp` a `v5.cpp`, que corresponden a las cinco versiones del algoritmo:
  - `v1.cpp`: Versión base, construcción Greedy secuencial sin reparación.
  - `v2.cpp`: Construcción con asignación paralela (round-robin) y una función de reparación avanzada.
  - `v3.cpp`: Construcción Greedy secuencial con una función de reparación alternativa.
  - `v4.cpp`: Versión con una tercera estrategia de reparación.
  - `v5.cpp`: Versión con una cuarta estrategia de reparación.

- `Instancias/`: Contiene las instancias del problema en formato `.txt`.

- `Outs/`: Carpeta generada automáticamente. Contiene una subcarpeta por versión (`v1`, `v2`, ..., `v5`) con los archivos `.out` correspondientes a los resultados de cada instancia.

- `bin/`: Ejecutables compilados de cada versión (`vrpb_v1`, ..., `vrpb_v5`).

- `Makefile`: Script para compilar, ejecutar y limpiar automáticamente todas las versiones.

---

## Compilar

```bash
make
```
---

## Ejecutar todas las instancias

```bash
make run
```
---

## Limpiar archivos generados

```bash
make clean
```