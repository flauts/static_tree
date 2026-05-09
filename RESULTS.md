# Resultados

Comando usado:

```powershell
.\build\static_tree_bench.exe --n 1000000 --queries 1000000 --trials 5 --block-size 64 --out results\results.csv
```

Promedio de 5 experimentos:

| Estructura | Build ms | Query ms | ns/query | Found |
| --- | ---: | ---: | ---: | ---: |
| Cache-oblivious static tree | 36.197 | 123.413 | 123.413 | 500000 |
| BST con punteros | 39.246 | 450.997 | 450.997 | 500000 |

En esta corrida el arbol estatico fue aproximadamente `450.997 / 123.413 = 3.65x` mas rapido en consultas.

El archivo completo con todos los trials quedo en `results/results.csv`.
