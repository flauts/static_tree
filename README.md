# static_tree

Implementacion simple para comparar:

- `cache_oblivious_static_tree`: BST estatico balanceado guardado en un arreglo con layout van Emde Boas.
- `pointer_bst`: BST balanceado usando nodos con punteros.

Ambos arboles se construyen con los mismos enteros `int32_t`. Las consultas tambien son las mismas para ambos. La mitad de las consultas son hits y la otra mitad son misses.

## Compilar

```powershell
mingw32-make all
```

Tambien se puede compilar directo:

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -pedantic src/main.cpp -o build/static_tree_bench.exe
```

## Ejecutar experimento

Caso pedido, con al menos 1M elementos y 1M consultas:

```powershell
.\build\static_tree_bench.exe --n 1000000 --queries 1000000 --trials 5 --block-size 64 --out results\results.csv
```

Parametros:

- `--n`: numero de elementos.
- `--queries`: numero de consultas.
- `--trials`: cantidad de repeticiones para promediar.
- `--block-size`: B objetivo en bytes. El layout cache-oblivious no se ajusta usando B, pero el valor queda guardado en el CSV para el reporte.
- `--seed`: semilla base.
- `--out`: archivo CSV de salida.

## Salida

El CSV guarda una fila por estructura y por trial, mas una fila `avg` con el promedio:

```text
trial,n,queries,block_size,structure,build_ms,query_ms,ns_per_query,found,hardware_threads,pointer_bits,compiler
```

`query_ms` es el tiempo usado solo en buscar las consultas. `build_ms` queda separado para ver tambien el costo de construccion.

## Hardware usado

Completar esta parte con la maquina donde se corran los resultados finales. En esta maquina de prueba se pudo leer:

- OS: Microsoft Windows NT 10.0.26200.0
- CPU identifier: AMD64 Family 25 Model 68 Stepping 1, AuthenticAMD
- Logical processors: 16
- Compiler: g++ 14.2.0
- Pointer size: 64 bits

WMI/systeminfo no estaban disponibles por permisos, asi que la RAM debe anotarse manualmente si el reporte la exige.

## Nota de implementacion

Primero se crea un BST logico balanceado a partir del arreglo ordenado. Para el arbol estatico se reordena ese arbol en memoria con una recursion van Emde Boas: se guarda la parte superior del arbol y luego sus subarboles inferiores. La busqueda sigue siendo la de un BST normal, pero los nodos cercanos en altura quedan mas cerca en memoria.
