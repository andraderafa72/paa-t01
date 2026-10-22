# Alocação de produtos no armazém

Heurística de custo incremental para posicionar produtos. Cada um ocupa uma posição com capacidade suficiente. O custo junta acesso e separação de itens do mesmo pedido.

Entre na pasta `PAA-T01`. Não há biblioteca extra.

## Compilar direto

```bash
c++ -std=c++17 -O2 -I lib -o paa_t01 \
  source/main.cpp source/program.cpp \
  lib/instance.cpp lib/cost.cpp \
  lib/greedy.cpp lib/local_search.cpp \
  lib/improved.cpp

c++ -std=c++17 -O2 -o gerar_instancias tools/gerar_instancias.cpp
./gerar_instancias
./paa_t01
```

## Compilar com CMake

A configuração padrão é Release.

```bash
cmake -S . -B build
cmake --build build
./build/gerar_instancias
./build/source/paa_t01
```

`gerar_instancias` grava `data/n20`, `data/n50`, `data/n100` e `data/n200`. Essas pastas já estão no repositório.

## Menu

1. Carregar instância. A lista mostra as pastas de `data/`. Digite o número.
2. Guloso por frequência.
3. Busca local por trocas.
4. Heurística aprimorada. Este é o algoritmo do trabalho.
5. Mostrar o último resultado.
6. Ver o log de resultados (`resultados/log.txt`).
7. Ajuda.
8. Sair.

A tela mostra o posicionamento e, abaixo da tabela, o custo e o tempo.

Ao fim de cada algoritmo o programa grava, em `resultados/<instancia>/`:

- `relatorio.txt` e `melhor-solucao.txt`, com o último algoritmo executado
- `relatorio-guloso.txt`, `relatorio-busca-local.txt` e `relatorio-heuristica.txt`
- `melhor-solucao-guloso.txt`, `melhor-solucao-busca-local.txt` e `melhor-solucao-heuristica.txt`

Cada execução também acrescenta uma linha em `resultados/log.txt`. A opção 6 mostra esse arquivo. Se o log ainda não existir, a opção 6 monta o arquivo a partir dos relatórios já gravados em `resultados/`.
