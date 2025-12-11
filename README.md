# SSC0713---2025.2---Grupo-18
Projeto para a disciplina SSC0713 - Sistemas Evolutivos Aplicados à Robótica

O projeto, que vai pelo nome de "   ", consiste de um programa em C++ que simula o jogo "Connect Four", no qual dois jogadores alternam entre si para depositar pedras em um grid afetado pela gravidade, buscando ligar quatro pedras suas consecutivas, treinando um algoritmo evolutivo simples (neste caso, cada indivíduo possui apenas 4 características, utilizadas para definir uma pontuação para cada movimento possível em uma dada rodada) por meio de torneios simples e seleção por elitismo (entende-se a possibilidade alta de overfitting que tal método gera, mas até o momento testes apresentaram resultados favoráveis contra jogadores humanos).


O genoma de cada indivíduo consiste dos valores atribuídos à proximidade do centro, formação de cadeias de 2 pedras consecutivas, formação de cadeias de 3 pedras consecutivas, e bloqueio de cadeias do adversário. Movimentos que ganham a partida ou que permitem que o oponente ganhe a partida não possuem seus próprios parâmetros e são sempre priorizados. Ademais, é possível configurar o algoritmo para tomar suas decisões com profundidade de múltiplas rodadas. O treinamento, para fins de velocidade, ocorre em profundidade menor que as partidas contra humanos. 

Para geração da interface visual, a biblioteca "raylib.h" foi utilizada.
https://raysan5.itch.io/raylib

<img width="1195" height="513" alt="image" src="https://github.com/user-attachments/assets/a10c4522-8d7a-41ea-a8ff-7d263138a5be" />

Na interface, o usuário é apresentado uma partida aleatória de cada geração
