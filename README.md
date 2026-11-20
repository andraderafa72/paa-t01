# Alocação de produtos no armazém

Heurística de custo incremental para posicionar produtos. Cada um ocupa uma posição com capacidade suficiente. O custo junta acesso e separação de itens do mesmo pedido.

Entre na pasta `PAA-T01`. Não há biblioteca extra.

## Compilar direto

```bash
c++ -std=c++17 -O2 -I lib -o paa_t01 \
  source/main.cpp source/program.cpp \
  lib/instance.cpp lib/cost.cpp \
  lib/greedy/greedy.cpp lib/local_search/local_search.cpp \
  lib/improved/improved.cpp \
  lib/branch_and_bound/assignment.cpp lib/branch_and_bound/branch_and_bound.cpp

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

`gerar_instancias` grava `data/n8`, `data/n10`, `data/n12`, `data/n15`, `data/n20`, `data/n50`, `data/n100` e `data/n200`. Essas pastas já estão no repositório.

## Menu

1. Carregar instância. A lista mostra as pastas de `data/`. Digite o número.
2. Guloso por frequência.
3. Busca local por trocas.
4. Heurística aprimorada. Este é o algoritmo do trabalho.
5. Branch and Bound. Usa a heurística como teto e prova o ótimo. O programa pergunta o limite em segundos; Enter segue até o ótimo. 1200 são 20 minutos.
6. Comparar guloso, busca local, heurística e o exato em `armazem`, `n8`, `n10`, `n12`, `n15` e `n20`, com o mesmo limite.
7. Mostrar o último resultado.
8. Ver o log de resultados (`resultados/log.txt`).
9. Ajuda.
10. Sair.

A tela mostra o posicionamento e, abaixo da tabela, o custo e o tempo.

Ao fim de cada algoritmo o programa grava, em `resultados/<instancia>/`:

- `relatorio.txt` e `melhor-solucao.txt`, com o último algoritmo executado
- `relatorio-guloso.txt`, `relatorio-busca-local.txt`, `relatorio-heuristica.txt` e `relatorio-branch-and-bound.txt`
- `melhor-solucao-guloso.txt`, `melhor-solucao-busca-local.txt`, `melhor-solucao-heuristica.txt` e `melhor-solucao-branch-and-bound.txt`

A opção 6 grava `resultados/comparacao.txt`. O gap é `(custo - ótimo) / ótimo * 100`. Sem prova de otimalidade, o gap fica contra o incumbente.

Cada execução também acrescenta uma linha em `resultados/log.txt`. A opção 6 mostra esse arquivo. Se o log ainda não existir, a opção 6 monta o arquivo a partir dos relatórios já gravados em `resultados/`.
