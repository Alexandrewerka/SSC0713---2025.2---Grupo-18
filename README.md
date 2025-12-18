# Connect 4 Evolutivo - Sistemas Evolutivos Aplicados à Robótica
Projeto final do Grupo 18 para a disciplina SSC0713 - Sistemas Evolutivos Aplicados à Robótica, ministrada pelo Prof. Dr. Eduardo do Valle Simões na Universidade de São Paulo (USP).

### Vídeo de Apresentação do Projeto: 
https://drive.google.com/file/d/1whdOt2_Skn7THp_MxbgGqt5Rzq5wfNsh/view?usp=drive_link

## Sobre o Projeto
O projeto, que vai pelo nome de "Connect 4 Evolutivo", consiste de um programa em C++ que simula o jogo "Connect Four", no qual dois jogadores alternam entre si para depositar pedras em um grid afetado pela gravidade, buscando ligar quatro pedras suas consecutivas, treinando um algoritmo evolutivo simples (neste caso, cada indivíduo possui apenas 4 características, utilizadas para definir uma pontuação para cada movimento possível em uma dada rodada) por meio de torneios simples (atribuindo uma pontuação positiva no valor de fitness para cada vitória e uma penalidade para cada derrota) e seleção por elitismo (entende-se a possibilidade alta de overfitting que tal método gera, mas até o momento testes apresentaram resultados favoráveis contra jogadores humanos).


O genoma de cada indivíduo consiste dos valores atribuídos à proximidade do centro, formação de cadeias de 2 pedras consecutivas, formação de cadeias de 3 pedras consecutivas, e bloqueio de cadeias do adversário. Movimentos que ganham a partida ou que permitem que o oponente ganhe a partida não possuem seus próprios parâmetros e são sempre priorizados/evitados. A lógica do algoritmo é governada por um processo de poda alpha-beta, que deixa de analisar seções do tabuleiro que apresentaram pontuação ruim nas jogadas simuladas anteriores, para que o uso do modelo em profundidades maiores não seja tão vagaroso. O treinamento, para fins de velocidade, ocorre em profundidade menor que as partidas contra humanos. 

Para geração da interface visual, a biblioteca "raylib.h" foi utilizada.
https://raysan5.itch.io/raylib

<img width="1195" height="513" alt="image" src="https://github.com/user-attachments/assets/a10c4522-8d7a-41ea-a8ff-7d263138a5be" />

Na interface, o usuário é apresentado uma partida aleatória de cada geração durante o período de teste, e é inceremoniosamente e violentamente depositado em uma partida contra o melhor indivíduo encontrado, onde poderá jogar repetidamente contra o modelo treinado até desistir (ou talvez vencer, mas ainda não conseguimos isso).

## Implementação e Aprendizados

Abaixo, detalhamos a estrutura do código, as estratégias adotadas e as principais dificuldades que encontramos (e resolvemos) durante o desenvolvimento.

### 1. Estrutura do Agente e Genoma
Em vez de utilizar redes neurais complexas, optamos por uma abordagem direta onde o AG otimiza os pesos de uma função heurística. Definimos o `Agent` com um **genoma composto por apenas 4 genes (pesos)**, que representam características estratégicas do jogo:

1.  **Controle de Centro:** Peso dado para peças na coluna central.
2.  **Sequência de 2:** Peso para alinhar duas peças.
3.  **Sequência de 3:** Peso para alinhar três peças (preparação para vitória).
4.  **Bloqueio:** Peso negativo (punição) caso o oponente tenha uma sequência de três peças não bloqueada.

**Otimização de estratégia:** Durante os testes iniciais, notamos que o agente jogava muito nas bordas e perdia rapidamente. Observamos que no Connect 4, estatisticamente, **quem controla a coluna do meio tem mais chances de vitória**. Com base nisso, alteramos a ordem de varredura do tabuleiro no código (`getOptimizedMoves`), forçando o algoritmo a priorizar a análise do centro antes das bordas. Isso melhorou significativamente o desempenho e a velocidade da poda Alpha-Beta.

### 2. Minimax e Otimizações (V11)
Os pesos do genoma são utilizados por um algoritmo **Minimax com Poda Alpha-Beta** para decidir a jogada. A poda é fundamental para descartar ramos da árvore de decisão que não precisam ser calculados, melhorando a performance.

Tivemos uma dificuldade onde o AG, mesmo "ganhando", às vezes ignorava uma vitória imediata ou deixava passar um bloqueio óbvio porque a profundidade da busca o fazia focar em um ganho futuro incerto.
Para corrigir isso, implementamos uma verificação de **Vitória/Derrota Imediata** antes da recursão do Minimax. Se o agente pode ganhar na próxima jogada, ele joga imediatamente. Se vai perder, ele bloqueia. Isso tornou o comportamento do AG muito mais consistente e agressivo.

### 3. O Processo Evolutivo
O treinamento ocorre através de torneios.

* **População:** 50 agentes gerados com pesos aleatórios.
* **Fitness:** Calculado com base em 3 partidas por indivíduo. Vitória soma pontos (+20), derrota subtrai (-20).
* **Seleção por Elitismo:** Optamos por manter os **Top 6** indivíduos de cada geração inalterados. Percebemos que, sem o elitismo, o crossover acabava "diluindo" boas estratégias encontradas, fazendo o AG regredir em alguns momentos.
* **Mutação:** Aplicamos uma taxa de 20% para variar levemente os pesos e evitar ótimos locais.

### 4. Stack Tecnológica e Performance
* **Linguagem:** C++, escolhido pela necessidade de velocidade nos cálculos recursivos do Minimax.
* **Visualização:** [Raylib](https://www.raylib.com/), utilizada para renderizar tanto o `GAMEPLAY` quanto o `BATCH_PLAYBACK` (replay dos treinos).
* **Profundidade Adaptativa:** Para viabilizar o tempo de treino, as gerações evoluem com uma profundidade menor ("Depth 2" ou "4"). Na partida final contra o usuário, aumentamos a profundidade (até 9), permitindo que o AG utilize os pesos otimizados com uma capacidade de previsão muito maior.

## Resultados e Análise da Evolução

Para monitorar o aprendizado do algoritmo em tempo real, implementamos um sistema de plotagem gráfica dentro da interface. O gráfico abaixo ilustra a curva de convergência durante um treinamento típico de 50 gerações.

<div align="center">
<img width="800" alt="Gráfico de Convergência Evolutiva" src="https://github.com/user-attachments/assets/5b6e85b6-bb4d-4108-b5a5-b4c7ae6df7e1" />
</div>

**Interpretação dos Dados:**

* **Linha Verde (Best):** Representa o fitness do melhor indivíduo de cada geração. O gráfico demonstra uma rápida convergência para o teto de pontuação (aprox. 60 pontos), o que indica que o campeão vence consistentemente suas 3 partidas de torneio.
* **Linha Amarela (Avg):** Representa a média de fitness da população. Ela permanece próxima de zero, o que é esperado estatisticamente em um sistema de torneio fechado (soma zero), onde para cada agente que ganha pontos (+20), outro é penalizado na mesma proporção (-20).

**Observação sobre Robustez:**
É possível notar pequenas quedas ("vales") na linha verde em determinados momentos. Isso ocorre quando mutações ou pareamentos desfavoráveis afetam momentaneamente o desempenho. No entanto, o gráfico evidencia a eficácia do método de **Seleção por Elitismo**: a recuperação é imediata na geração seguinte, garantindo que as melhores estratégias (pesos otimizados) sejam preservadas e o AG não regrida.

## Projeto desenvolvido por:
* *Alexandre Bueno Zapaterra Filho, NºUSP 15746448*
* *Alexandre Werka Vieira, NºUSP 15437029*
* *Lucas Iglesias Ribeiro Siqueira, NºUSP 15639549*
