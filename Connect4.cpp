#include "raylib.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <limits>
#include <string>

using namespace std;

// Dimensões do tabuleiro padrão de Connect 4
const int ROWS = 6;
const int COLS = 7;

// Configurações Visuais (Raylib)
const int CELL_SIZE = 90;
const int SCREEN_WIDTH = 1200;
const int SCREEN_HEIGHT = 800;
// Configurações para o tabuleiro miniatura (visto durante o replay do treino)
const int MINI_CELL_SIZE = 30;
const int MINI_BOARD_W = COLS * MINI_CELL_SIZE;
const int MINI_BOARD_H = ROWS * MINI_CELL_SIZE;

const int POPULATION_SIZE = 50;
const int GENERATIONS_PER_BATCH = 10;
const int TOTAL_BATCHES = 5;
const double MUTATION_RATE = 0.20;

Color BLUE_BOARD = { 0, 80, 180, 255 };
Color RED_P1 = { 230, 40, 55, 255 };
Color YEL_P2 = { 255, 220, 0, 255 };
Color BG_COLOR = { 20, 20, 20, 255 };

// Gerador de números aleatórios moderno (melhor que rand() puro para distribuição)
random_device rd;
mt19937 gen(rd());

// Estados da Aplicação (Máquina de Estados)
enum AppState {
    MENU,           // Tela inicial de seleção
    BATCH_CALC,     // Tela de carregamento
    BATCH_PLAYBACK, // Mostra o replay das gerações passadas
    GAMEPLAY        // Humano vs A Melhor AG encontrada
};

// Níveis de Dificuldade controlam a profundidade (DEPTH) do Minimax
// Quanto maior a profundidade, mais jogadas à frente a AG vê (e mais lento fica).
enum Difficulty {
    EASY = 2,
    MEDIUM = 4,
    HARD = 7,
    IMPOSSIBLE = 8
};

// Tabuleiro
class Board {
public:
    int grid[ROWS][COLS]; // 0 = Vazio, 1 = Pedra do P1, 2 = Pedra do P2

    Board() { reset(); }

    // Limpa o tabuleiro
    void reset() { for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) grid[r][c] = 0; }

    // Copia o estado de outro tabuleiro (para simulação do Minimax)
    void copyFrom(const Board& o) { for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) grid[r][c] = o.grid[r][c]; }

    // Verifica se a coluna não está cheia
    bool isValid(int c) const { return c >= 0 && c < COLS && grid[0][c] == 0; }

    // Encontra a linha mais baixa disponível em uma coluna (simula a gravidade)
    int getRowForCol(int c) const {
        for (int r = ROWS - 1; r >= 0; r--) if (grid[r][c] == 0) return r;
        return -1;
    }

    // Coloca a peça na coluna 'c'
    void drop(int c, int p) {
        int r = getRowForCol(c);
        if (r != -1) grid[r][c] = p;
    }

    // Verifica todas as direções para ver se alguém ganhou
    bool checkWin(int p) const {
        // Horizontal
        for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS - 3; c++) if (grid[r][c] == p && grid[r][c + 1] == p && grid[r][c + 2] == p && grid[r][c + 3] == p) return true;
        // Vertical
        for (int r = 0; r < ROWS - 3; r++) for (int c = 0; c < COLS; c++) if (grid[r][c] == p && grid[r + 1][c] == p && grid[r + 2][c] == p && grid[r + 3][c] == p) return true;
        // Diagonal Principal
        for (int r = 0; r < ROWS - 3; r++) for (int c = 0; c < COLS - 3; c++) if (grid[r][c] == p && grid[r + 1][c + 1] == p && grid[r + 2][c + 2] == p && grid[r + 3][c + 3] == p) return true;
        // Diagonal Secundária
        for (int r = 3; r < ROWS; r++) for (int c = 0; c < COLS - 3; c++) if (grid[r][c] == p && grid[r - 1][c + 1] == p && grid[r - 2][c + 2] == p && grid[r - 3][c + 3] == p) return true;
        return false;
    }

    // Verifica empate (tabuleiro cheio)
    bool isFull() const { for (int c = 0; c < COLS; c++) if (grid[0][c] == 0) return false; return true; }

    // OTIMIZAÇÃO PARA MINIMAX (Poda Alpha-Beta):
    // O algoritmo deve verificar o centro do tabuleiro primeiro
    // porque o centro oferece mais oportunidades de vitória, permitindo
    // que a "poda" (corte de cálculos desnecessários) aconteça mais cedo
    vector<int> getOptimizedMoves() const {
        vector<int> moves;
        int order[] = { 3, 2, 4, 1, 5, 0, 6 }; // Ordem: Meio -> Bordas
        for (int c : order) {
            if (grid[0][c] == 0) moves.push_back(c);
        }
        return moves;
    }
};

// Algoritmo Genético (Agente)
struct Agent {
    // genome[0] = Valor de controlar o centro
    // genome[1] = Valor de ter 2 peças alinhadas
    // genome[2] = Valor de ter 3 peças alinhadas
    // genome[3] = Medo de o oponente ter 3 peças
    vector<double> genome;
    double fitness;

    // Cria um agente com genoma aleatório (para o início da evolução)
    Agent() {
        uniform_real_distribution<double> d(-10.0, 10.0);
        genome = { d(gen), d(gen), d(gen), d(gen) };
        fitness = 0;
    }

    Agent(vector<double> g) { genome = g; fitness = 0; }

    // Analisa 4 células consecutivas e atribui uma pontuação baseada nos genes
    double evalWindow(const vector<int>& w, int piece) {
        double score = 0;
        int opp = (piece == 1) ? 2 : 1; // Define quem é o oponente
        int cp = 0, ce = 0, co = 0; // Contadores: Peça, Vazio, Oponente

        for (int x : w) { if (x == piece) cp++; else if (x == 0) ce++; else co++; }

        // Prioriza a vitória sempre que possível
        if (cp == 4) score += 1000000;
        // Favorece conectar 3 pedras
        else if (cp == 3 && ce == 1) score += genome[2] * 100.0;
        // Favorece (menos) conectar 2 pedras
        else if (cp == 2 && ce == 2) score += genome[1] * 10.0;

        // Tenta evitar que o oponente conecte 3 pedras
        if (co == 3 && ce == 1) score -= genome[3] * 500.0;

        return score;
    }

    // Percorre todo o tabuleiro somando os pontos de cada jogada
    double scoreBoard(const Board& b, int piece) {
        double score = 0;

        // 1. Controle de Centro - Peças no meio valem mais
        int centerCol = COLS / 2;
        int centerCount = 0;
        for (int r = 0; r < ROWS; r++) {
            if (b.grid[r][centerCol] == piece) centerCount++;
        }
        score += centerCount * genome[0] * 15.0;

        // 2. Scan de Janelas (Horizontal, Vertical, Diagonais)
        // Isso passa "janelas deslizantes" de 4 espaços pelo tabuleiro inteiro
        for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS - 3; c++) score += evalWindow({ b.grid[r][c], b.grid[r][c + 1], b.grid[r][c + 2], b.grid[r][c + 3] }, piece);
        for (int c = 0; c < COLS; c++) for (int r = 0; r < ROWS - 3; r++) score += evalWindow({ b.grid[r][c], b.grid[r + 1][c], b.grid[r + 2][c], b.grid[r + 3][c] }, piece);
        for (int r = 0; r < ROWS - 3; r++) for (int c = 0; c < COLS - 3; c++) score += evalWindow({ b.grid[r][c], b.grid[r + 1][c + 1], b.grid[r + 2][c + 2], b.grid[r + 3][c + 3] }, piece);
        for (int r = 3; r < ROWS; r++) for (int c = 0; c < COLS - 3; c++) score += evalWindow({ b.grid[r][c], b.grid[r - 1][c + 1], b.grid[r - 2][c + 2], b.grid[r - 3][c + 3] }, piece);

        return score;
    }

    // ALGORITMO MINIMAX
    // alpha/beta: Variáveis para "podar" (ignorar) jogadas ruins
    // maximizing: valor booleano que indica se estamos maximizando (AG) ou minimizando a pontuação (oponente)
    pair<int, double> minimax(Board& b, int depth, double alpha, double beta, bool maximizing, int piece) {
        // Verifica condições de fim de jogo
        bool term = b.checkWin(1) || b.checkWin(2) || b.isFull();
        int opp = (piece == 1) ? 2 : 1;

        if (depth == 0 || term) {
            if (term) {
                // Pontuações para vitória/derrota
                if (b.checkWin(piece)) return { -1, 100000000.0 };
                else if (b.checkWin(opp)) return { -1, -100000000.0 };
                else return { -1, 0 }; // Empate
            }
            // Se não acabou, retorna o mapa de pontuação
            return { -1, scoreBoard(b, piece) };
        }

        vector<int> moves = b.getOptimizedMoves(); // Pega movimentos ordenados (Centro -> Bordas)

        if (maximizing) { // Vez da AG
            double maxEval = -numeric_limits<double>::infinity();
            int bestCol = (moves.size() > 0) ? moves[0] : -1;
            for (int col : moves) {
                Board tmp; tmp.copyFrom(b); tmp.drop(col, piece); // Simula jogada
                double eval = minimax(tmp, depth - 1, alpha, beta, false, piece).second; // Recursão
                if (eval > maxEval) { maxEval = eval; bestCol = col; }
                alpha = max(alpha, eval);
                if (beta <= alpha) break; // PODA ALPHA-BETA: Corta o loop se achar caminho melhor
            }
            return { bestCol, maxEval };
        }
        else { // Vez do Oponente (tenta minimizar o score da AG)
            double minEval = numeric_limits<double>::infinity();
            int bestCol = (moves.size() > 0) ? moves[0] : -1;
            for (int col : moves) {
                Board tmp; tmp.copyFrom(b); tmp.drop(col, opp); // Simula jogada
                double eval = minimax(tmp, depth - 1, alpha, beta, true, piece).second; // Recurso
                if (eval < minEval) { minEval = eval; bestCol = col; }
                beta = min(beta, eval);
                if (beta <= alpha) break; // PODA
            }
            return { bestCol, minEval };
        }
    }
};

// Variáveis Globais
vector<Agent> population;
Agent bestOfAllTime;
int currentBatch = 0;
int globalGenCounter = 0;
int selectedDifficulty = MEDIUM;

// Estrutura para salvar o replay visual
struct MiniMatch {
    Board board;
    Agent p1; Agent p2;
    int turn; bool gameOver; int genID; float timer;
};
vector<MiniMatch> batchMatches;

// Reseta a população para o estado inicial
void InitPopulation() {
    population.clear();
    for (int i = 0; i < POPULATION_SIZE; i++) population.push_back(Agent());
    currentBatch = 0;
    globalGenCounter = 0;
}

// Processo Evolutivo
Agent EvolveOneGeneration() {
    for (int i = 0; i < POPULATION_SIZE; i++) {
        population[i].fitness = 0;
        int matches = 3; // Cada agente joga 3 partidas
        for (int k = 0; k < matches; k++) {
            int oppIdx = rand() % POPULATION_SIZE; // Oponente aleatório
            Board b;
            int moves = 0; bool over = false; int turn = 1;

            // Loop da partida de treino - Interrompe se demorar demais
            while (!over && moves < 30) {
                int piece = (turn == 1) ? 1 : 2;

                // Durante o treino, usamos Depth 4 fixo
                // Se fosse muito baixo (1 ou 2), a evolução seria rápida mas a AG seria burra
                // Se fosse muito alto (6+), demoraria demais para treinar
                int col = (turn == 1) ? population[i].minimax(b, 4, -9e9, 9e9, true, 1).first
                    : population[oppIdx].minimax(b, 4, -9e9, 9e9, true, 2).first;

                if (b.isValid(col)) {
                    b.drop(col, piece);
                    if (b.checkWin(piece)) {
                        // Recompensa e Punição
                        if (turn == 1) population[i].fitness += 20;
                        else population[i].fitness -= 20;
                        over = true;
                    }
                }
                if (b.isFull()) over = true;
                turn = (turn == 1) ? 2 : 1;
                moves++;
            }
        }
    }

    // Seleção
    // Ordena de acordo com a fitness
    sort(population.begin(), population.end(), [](Agent& a, Agent& b) { return a.fitness > b.fitness; });
    Agent champion = population[0];

    // Reprodução
    vector<Agent> newPop;

    // Elitismo: Mantém os 10 melhores para a próxima geração
    for (int i = 0; i < 10; i++) newPop.push_back(population[i]);

    // Preenche o resto da população com filhos
    while (newPop.size() < POPULATION_SIZE) {
        // Pais são escolhidos apenas do terço superior da população
        int p1 = rand() % (POPULATION_SIZE / 3);
        int p2 = rand() % (POPULATION_SIZE / 3);

        vector<double> childG;
        // Crossover Uniforme: Para cada gene, escolhe aleatoriamente se vem do pai ou da mãe
        for (int g = 0; g < 4; g++) {
            double v = (rand() % 2 == 0) ? population[p1].genome[g] : population[p2].genome[g];

            // Mutação
            if ((rand() % 100) < (MUTATION_RATE * 100)) v += ((rand() % 100) / 20.0 - 2.5);
            childG.push_back(v);
        }
        newPop.push_back(Agent(childG));
    }
    population = newPop; // Substitui a população antiga pela nova
    return champion;
}

// Função para desenhar os tabuleiros pequenos na tela de replay
void DrawMiniBoard(MiniMatch& m, int x, int y) {
    DrawRectangle(x, y, MINI_BOARD_W, MINI_BOARD_H + 20, BLACK);
    DrawText(TextFormat("Gen %d", m.genID), x + 5, y + MINI_BOARD_H + 2, 10, WHITE);
    Color status = m.gameOver ? RED : GREEN;
    DrawCircle(x + MINI_BOARD_W - 10, y + MINI_BOARD_H + 8, 4, status);
    for (int c = 0; c < COLS; c++) {
        for (int r = 0; r < ROWS; r++) {
            int px = x + c * MINI_CELL_SIZE;
            int py = y + r * MINI_CELL_SIZE;
            DrawRectangle(px, py, MINI_CELL_SIZE, MINI_CELL_SIZE, BLUE_BOARD);
            DrawCircle(px + MINI_CELL_SIZE / 2, py + MINI_CELL_SIZE / 2, MINI_CELL_SIZE / 2 - 2, BG_COLOR);
            if (m.board.grid[r][c] == 1) DrawCircle(px + MINI_CELL_SIZE / 2, py + MINI_CELL_SIZE / 2, MINI_CELL_SIZE / 2 - 2, RED_P1);
            if (m.board.grid[r][c] == 2) DrawCircle(px + MINI_CELL_SIZE / 2, py + MINI_CELL_SIZE / 2, MINI_CELL_SIZE / 2 - 2, YEL_P2);
        }
    }
}

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Evolutionary Connect 4");
    SetTargetFPS(60);

    AppState state = MENU;

    // Variáveis para o jogo
    Board playBoard;
    int playTurn = 1; bool playOver = false; bool aiThinking = false;
    float aiTimer = 0.0f; int winner = 0;
    float batchResetTimer = 0.0f;

    // Loop Principal
    while (!WindowShouldClose()) {
        float dt = GetFrameTime(); // Tempo entre frames

        switch (state) {
            // Máquina de Estados Finitos - Alterna entre diferentes estados
        case MENU:
            // Lógica de seleção de dificuldade
            if (IsKeyPressed(KEY_DOWN)) {
                if (selectedDifficulty == EASY) selectedDifficulty = MEDIUM;
                else if (selectedDifficulty == MEDIUM) selectedDifficulty = HARD;
                else if (selectedDifficulty == HARD) selectedDifficulty = IMPOSSIBLE;
                else selectedDifficulty = EASY;
            }
            if (IsKeyPressed(KEY_UP)) {
                if (selectedDifficulty == EASY) selectedDifficulty = IMPOSSIBLE;
                else if (selectedDifficulty == MEDIUM) selectedDifficulty = EASY;
                else if (selectedDifficulty == HARD) selectedDifficulty = MEDIUM;
                else selectedDifficulty = HARD;
            }

            // Desenho do Menu
            BeginDrawing();
            ClearBackground(BG_COLOR);
            DrawText("EVOLUTIONARY CONNECT 4", 300, 100, 40, WHITE);
            DrawText("Escolha a Dificuldade:", 350, 250, 20, LIGHTGRAY);

            // Mostra a opção selecionada
            DrawText("FACIL (Depth 2)", 500, 300, 30, (selectedDifficulty == EASY) ? GREEN : DARKGRAY);
            DrawText("MEDIO (Depth 4)", 500, 350, 30, (selectedDifficulty == MEDIUM) ? GREEN : DARKGRAY);
            DrawText("DIFICIL (Depth 7)", 500, 400, 30, (selectedDifficulty == HARD) ? GREEN : DARKGRAY);
            DrawText("IMPOSSIVEL (Depth 8)", 500, 450, 30, (selectedDifficulty == IMPOSSIBLE) ? RED : DARKGRAY);

            DrawText("Pressione [ENTER] para Iniciar Treino", 320, 550, 20, WHITE);
            EndDrawing();

            if (IsKeyPressed(KEY_ENTER)) { InitPopulation(); state = BATCH_CALC; }
            break;

            // Estado 2 - Treinamento da AG
            // Jogo trava enquanto a CPU queima calculando milhares de partidas
        case BATCH_CALC: {
            batchMatches.clear();
            BeginDrawing(); ClearBackground(BG_COLOR);
            DrawText("Treinando Modelo (Depth 4)...", 350, 350, 30, YEL_P2);
            DrawText("Isso pode travar um pouco, aguarde.", 380, 400, 20, GRAY);
            EndDrawing();

            // Loop de evolução rápido
            for (int i = 0; i < GENERATIONS_PER_BATCH; i++) {
                globalGenCounter++;
                Agent champion = EvolveOneGeneration(); // Evolui!

                // Prepara uma partida de exibição com o melhor da geração
                MiniMatch mm; mm.board.reset(); mm.p1 = champion; mm.p2 = population[1];
                mm.turn = 1; mm.gameOver = false; mm.genID = globalGenCounter;
                mm.timer = (float)(rand() % 100) / 100.0f; // Offset aleatório para animação não ficar sincronizada demais
                batchMatches.push_back(mm);
            }

            // Salva o melhor agente globalmente para jogar contra o humano depois
            bestOfAllTime = batchMatches.back().p1;
            state = BATCH_PLAYBACK;
            batchResetTimer = 0.0f;
            break;
        }

                       // Visualização
                       // Mostra uma grade com várias partidas
        case BATCH_PLAYBACK: {
            BeginDrawing();
            ClearBackground(BG_COLOR);
            DrawText(TextFormat("LOTE %d / %d (Total 100 Gens)", currentBatch + 1, TOTAL_BATCHES), 20, 20, 30, WHITE);
            int activeCount = 0;

            // Percorre todas as partidas de exibição geradas
            for (int i = 0; i < batchMatches.size(); i++) {
                MiniMatch& m = batchMatches[i];
                // Calcula posição na grade
                int col = i % 5; int row = i / 5;
                int startX = 50 + col * (MINI_BOARD_W + 20); int startY = 100 + row * (MINI_BOARD_H + 50);

                // Lógica de atualização dos tabuleiros pequenos
                if (!m.gameOver) {
                    activeCount++;
                    m.timer += dt;
                    if (m.timer > 0.05f) { // Velocidade da animação
                        m.timer = 0.0f;
                        int playCol = -1;
                        // Usa profundidade baixa para a visualização ser rápida
                        if (m.turn == 1) playCol = m.p1.minimax(m.board, 2, -9e9, 9e9, true, 1).first;
                        else playCol = m.p2.minimax(m.board, 2, -9e9, 9e9, true, 2).first;

                        if (m.board.isValid(playCol)) {
                            m.board.drop(playCol, m.turn);
                            if (m.board.checkWin(m.turn)) m.gameOver = true;
                            else if (m.board.isFull()) m.gameOver = true;
                            else m.turn = (m.turn == 1) ? 2 : 1;
                        }
                    }
                }
                DrawMiniBoard(m, startX, startY);
            }

            // Se todas as mini-partidas acabaram, espera um pouco e vai para o próximo lote ou para o jogo final
            if (activeCount == 0) {
                batchResetTimer += dt;
                if (batchResetTimer > 1.0f) {
                    currentBatch++;
                    if (currentBatch < TOTAL_BATCHES) state = BATCH_CALC; // Volta a calcular
                    else { playBoard.reset(); state = GAMEPLAY; } // Vai pro jogo real
                }
            }
            EndDrawing();
            break;
        }

                           // Jogo Real
        case GAMEPLAY: {
            if (!playOver) {
                // Turno do Jogador
                if (playTurn == 1) {
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        int mx = GetMouseX();
                        // Calcula onde o mouse está em relação às colunas
                        int boardXStart = (SCREEN_WIDTH - (COLS * CELL_SIZE)) / 2;
                        if (mx > boardXStart && mx < boardXStart + COLS * CELL_SIZE) {
                            int col = (mx - boardXStart) / CELL_SIZE;
                            if (playBoard.isValid(col)) {
                                playBoard.drop(col, 1);
                                if (playBoard.checkWin(1)) { playOver = true; winner = 1; }
                                else if (playBoard.isFull()) { playOver = true; winner = 0; }
                                else { playTurn = 2; aiThinking = true; aiTimer = 0; }
                            }
                        }
                    }
                }
                else if (aiThinking) {
                    // Turno da AG
                    aiTimer += dt;
                    if (aiTimer > 0.1f) { // Pequeno delay para parecer que está pensando
                        // Usa a dificuldade escolhida e o melhor agente treinado
                        int col = bestOfAllTime.minimax(playBoard, selectedDifficulty, -9e9, 9e9, true, 2).first;
                        if (playBoard.isValid(col)) {
                            playBoard.drop(col, 2);
                            if (playBoard.checkWin(2)) { playOver = true; winner = 2; }
                            else if (playBoard.isFull()) { playOver = true; winner = 0; }
                            else playTurn = 1;
                        }
                        aiThinking = false;
                    }
                }
            }

            // Desenho do Jogo Principal
            BeginDrawing();
            ClearBackground(BG_COLOR);
            const char* diff;
            if (selectedDifficulty == EASY) diff = "FACIL";
            else if (selectedDifficulty == MEDIUM) diff = "MEDIO";
            else if (selectedDifficulty == HARD) diff = "DIFICIL";
            else diff = "IMPOSSIVEL";

            DrawText(TextFormat("VOCE vs AG %s", diff), 400, 20, 30, WHITE);

            // Desenha o tabuleiro grande
            int bx = (SCREEN_WIDTH - (COLS * CELL_SIZE)) / 2; int by = 100;
            for (int c = 0; c < COLS; c++) for (int r = 0; r < ROWS; r++) {
                DrawRectangle(bx + c * CELL_SIZE, by + r * CELL_SIZE, CELL_SIZE, CELL_SIZE, BLUE_BOARD);
                Color pc = BG_COLOR;
                if (playBoard.grid[r][c] == 1) pc = RED_P1;
                if (playBoard.grid[r][c] == 2) pc = YEL_P2;
                DrawCircle(bx + c * CELL_SIZE + CELL_SIZE / 2, by + r * CELL_SIZE + CELL_SIZE / 2, CELL_SIZE / 2 - 5, pc);
            }

            // Tela de Fim de Jogo
            if (playOver) {
                DrawRectangle(0, SCREEN_HEIGHT / 2 - 50, SCREEN_WIDTH, 100, Fade(BLACK, 0.8f));
                const char* t = (winner == 1) ? "VOCE VENCEU!" : (winner == 2 ? "AG VENCEU!" : "EMPATE");
                DrawText(t, 500, SCREEN_HEIGHT / 2 - 20, 50, (winner == 1 ? RED_P1 : YEL_P2));

                if (IsKeyPressed(KEY_R)) { playBoard.reset(); playOver = false; playTurn = 1; }
                if (IsKeyPressed(KEY_M)) { state = MENU; }
                DrawText("[R] Reiniciar   [M] Menu", 500, SCREEN_HEIGHT / 2 + 40, 20, WHITE);
            }
            EndDrawing();
            break;
        }
        }
    }
    CloseWindow();
    return 0;
}