# Resultados

Comando usado:

```powershell
.\build\static_tree_bench.exe --n 1000000 --queries 1000000 --trials 5 --block-size 64 --out results\results.csv
```

Hardware registrado durante la corrida:

- OS: Microsoft Windows NT 10.0.26200.0
- CPU identifier: AMD64 Family 25 Model 68 Stepping 1, AuthenticAMD
- Logical processors: 16
- Compiler: g++ 14.2.0
- Pointer size: 64 bits

Promedio de 5 experimentos:

| Estructura | Build ms | Query ms | ns/query | Found |
| --- | ---: | ---: | ---: | ---: |
| Cache-oblivious static tree | 36.197 | 123.413 | 123.413 | 500000 |
| BST con punteros | 39.246 | 450.997 | 450.997 | 500000 |

En esta corrida el arbol estatico fue aproximadamente `450.997 / 123.413 = 3.65x` mas rapido en consultas.

El archivo completo con todos los trials quedo en `results/results.csv`.
