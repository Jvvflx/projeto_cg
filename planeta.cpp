// Etapa 2: planeta (esfera na origem) + Sol (esfera amarela na posição da luz)
//          + desenho do avião pousado na superfície do planeta,
//          visto por uma câmera em 3ª pessoa (atrás e acima do avião).
//          Poses do avião (temporário, teclas 1 a 5): parado, subindo,
//          descendo, virando à direita e virando à esquerda.
// Tecla R: alterna entre modo sólido e malha (wireframe). ESC: sair.
//
// Compilar no Linux:
// g++ -o planeta planeta.cpp -lglfw -lGL -lGLU
//
// Uso:  ./planeta <raio> <solX> <solY> <solZ>
// Ex.:  ./planeta 1.0 5 2 3
// Sem argumentos, os valores são lidos da entrada padrão.

// Compilar pelo makefile:
// make all

// Executar pelo makefile:
// make run

// Executar com parametros de teste pelo makefile:
// make run-args

// Limpa os arquivos gerados pelo makefile:
// make clean

// Limpa e recompila os arquivos gerados pelo makefile:
// make rebuild

#include <GL/gl.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <thread>
#include <cctype>
#include <cerrno>
#include <string>
#include <cstdio>

float raio = 1.0f;
float sol[3] = {5.0f, 2.0f, 3.0f};
float raioSol;            // tamanho do Sol, proporcional ao planeta
float tamanhoAviao;       // comprimento do avião, proporcional ao planeta
float planoDistante;      // limite do alcance de visão (plano far)

//estado do avião: a câmera em 3ª pessoa é derivada dele
float aviaoPos[3];                          // centro do aviao
float aviaoFrente[3] = {0.5f, 0.0f, 0.0f};  // direção do nariz
float aviaoCima[3]   = {0.0f, 0.5f, 0.0f};  // vetor cima do avião

bool malha = false;
const float FOV = 45.0f;

const double FPS_MAXIMO = 60.0;   // limite de quadros por segundo (limitador por software)

const float PROPORCAO_SOL   = 0.25f;  // raio do Sol em relação ao raio do planeta
const float PROPORCAO_AVIAO = 0.10f;  // comprimento do avião em relação ao raio do planeta
const float LIMITE_VALOR    = 1.0e6f; // maior valor absoluto aceito na entrada

//câmera em 3ª pessoa, em múltiplos do comprimento do avião
const float CAMERA_ATRAS   = 3.0f;    // quanto fica atrás do avião
const float CAMERA_ALTURA  = 1.2f;    // quanto fica acima do avião
const float CAMERA_ADIANTE = 2.0f;    // ponto mirado, à frente do avião

//poses do avião (por enquanto só o desenho; o movimento vem nas próximas etapas)
enum PoseAviao { POSE_PARADO, POSE_SUBINDO, POSE_DESCENDO, POSE_DIREITA, POSE_ESQUERDA };
PoseAviao poseAviao = POSE_PARADO;

const float ANGULO_PITCH     = 20.0f;  // inclinação do nariz ao subir/descer (graus)
const float ANGULO_ROLL = 30.0f;  // inclinação lateral das asas nas curvas (graus)
const float ANGULO_YAW    = 10.0f;  // quanto o nariz aponta para o lado da curva (graus)
const float ALTURA_PREVIA     = 1.0f;   // altura (em tamanhos de avião) usada só para ver as poses no ar

GLUquadric* quadrica;

//Converte o texto em exatamente n números reais finitos.
//Retorna false se faltar número, sobrar caractere, houver estouro
//de faixa ou valor não finito (nan, inf).
bool lerNumeros(const char* texto, float* valores, int n)
{
    const char* p = texto;

    for (int i = 0; i < n; i++)
    {
        char* fim;
        errno = 0;
        float v = std::strtof(p, &fim);

        //fim == p: nada foi convertido; ERANGE: não cabe em float
        if (fim == p || errno == ERANGE || !std::isfinite(v))
            return false;

        valores[i] = v;
        p = fim;
    }

    //depois dos n números só podem sobrar espaços
    while (std::isspace((unsigned char)*p))
        p++;

    return *p == '\0';
}

//Verifica se os parâmetros formam uma cena válida.
//Retorna NULL se estiver tudo certo, ou a mensagem de erro.
const char* validarCena(float raioPlaneta, const float posSol[3])
{
    if (raioPlaneta <= 0.0f)
        return "o raio do planeta deve ser positivo.";

    if (raioPlaneta > LIMITE_VALOR)
        return "o raio do planeta deve ser no maximo 1e6.";

    for (int i = 0; i < 3; i++)
        if (std::fabs(posSol[i]) > LIMITE_VALOR)
            return "as coordenadas do Sol devem estar entre -1e6 e 1e6.";

    //o Sol não pode tocar o planeta: distância entre os centros
    //tem de ser maior que a soma dos raios
    float distSol = std::sqrt(posSol[0]*posSol[0] + posSol[1]*posSol[1] + posSol[2]*posSol[2]);
    if (distSol <= raioPlaneta * (1.0f + PROPORCAO_SOL))
        return "o Sol esta dentro ou encostado no planeta; afaste-o do centro.";

    return NULL;
}

void mostrarUso(const char* programa)
{
    std::cerr << "Uso: " << programa << " <raio> <solX> <solY> <solZ>\n"
              << "  Ex.: " << programa << " 1.0 5 2 3\n"
              << "  Sem argumentos, os valores sao pedidos pelo teclado.\n"
              << "  Use ponto como separador decimal (1.5, nao 1,5).\n";
}

//Lê uma linha do teclado e converte em n números, repetindo até a
//entrada ser válida. Retorna false se a entrada terminar (Ctrl+D).
bool perguntarNumeros(const char* pergunta, float* valores, int n)
{
    std::string linha;

    while (true)
    {
        std::cout << pergunta;

        if (!std::getline(std::cin, linha))
            return false;

        if (lerNumeros(linha.c_str(), valores, n))
            return true;

        std::cerr << "Entrada invalida: digite " << n
                  << (n == 1 ? " numero" : " numeros separados por espaco")
                  << " (use ponto como separador decimal).\n";
    }
}

//Obtém raio e posição do Sol pelos argumentos ou pelo teclado.
//Retorna false se não for possível obter uma cena válida.
bool obterParametros(int argc, char** argv)
{
    //modo por argumentos: ./planeta <raio> <solX> <solY> <solZ>
    if (argc == 5)
    {
        float valores[4];

        for (int i = 0; i < 4; i++)
        {
            if (!lerNumeros(argv[i + 1], &valores[i], 1))
            {
                std::cerr << "Erro: argumento invalido '" << argv[i + 1] << "'.\n";
                mostrarUso(argv[0]);
                return false;
            }
        }

        raio   = valores[0];
        sol[0] = valores[1];
        sol[1] = valores[2];
        sol[2] = valores[3];

        const char* erro = validarCena(raio, sol);
        if (erro)
        {
            std::cerr << "Erro: " << erro << "\n";
            return false;
        }
        return true;
    }

    //qualquer quantidade diferente de 0 ou 4 argumentos é erro
    if (argc != 1)
    {
        std::cerr << "Erro: numero de argumentos incorreto.\n";
        mostrarUso(argv[0]);
        return false;
    }

    //modo interativo: pergunta até obter uma cena válida
    while (true)
    {
        if (!perguntarNumeros("Raio do planeta: ", &raio, 1) ||
            !perguntarNumeros("Posicao do Sol (x y z): ", sol, 3))
        {
            std::cerr << "\nEntrada encerrada.\n";
            return false;
        }

        const char* erro = validarCena(raio, sol);
        if (!erro)
            return true;

        std::cerr << "Erro: " << erro << " Tente novamente.\n\n";
    }
}

void iniciar()
{
   //define a cor de limpeza
   glClearColor(0.02, 0.02, 0.05, 1.0);

   //modo de desenho dos polígonos
   glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

   //modo de sombreamento
   glShadeModel(GL_SMOOTH);

   //Tamanhos proporcionais ao planeta
   float distSol = std::sqrt(sol[0]*sol[0] + sol[1]*sol[1] + sol[2]*sol[2]);
   raioSol = PROPORCAO_SOL * raio;
   tamanhoAviao = PROPORCAO_AVIAO * raio;

   //Avião pousado no topo do planeta.
   aviaoPos[0] = 0.0f;
   aviaoPos[1] = raio;
   aviaoPos[2] = 0.0f;

   //Alcance de visão: cobre o planeta inteiro e o Sol
   planoDistante = 2.0f * (distSol + raio);

   //Iluminação: o Sol é a fonte de luz (ambiente 0.15 + difusa 0.85)
   float ambienteGlobal[] = {0.15f, 0.15f, 0.15f, 1.0f};
   float difusa[]         = {0.85f, 0.85f, 0.85f, 1.0f};
   float ambienteLuz[]    = {0.0f, 0.0f, 0.0f, 1.0f};

   glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambienteGlobal);
   glLightfv(GL_LIGHT0, GL_AMBIENT, ambienteLuz);
   glLightfv(GL_LIGHT0, GL_DIFFUSE, difusa);
   glEnable(GL_LIGHT0);

   //glColor passa a definir o material (ambiente e difusa)
   glEnable(GL_COLOR_MATERIAL);
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

   //as peças do avião são escaladas (glScalef): renormaliza as normais
   //para a iluminação continuar correta
   glEnable(GL_NORMALIZE);

   //objeto usado por gluSphere
   quadrica = gluNewQuadric();
   gluQuadricNormals(quadrica, GLU_SMOOTH);
}

//Desenha um paralelepípedo centrado em (cx, cy, cz) com dimensões
//(sx, sy, sz). Usado para asas, estabilizador e deriva do avião.
void desenharCaixa(float cx, float cy, float cz, float sx, float sy, float sz)
{
    glPushMatrix();
    glTranslatef(cx, cy, cz);
    glScalef(sx, sy, sz);

    glBegin(GL_QUADS);
        //face +X
        glNormal3f(1, 0, 0);
        glVertex3f( 0.5f, -0.5f,  0.5f); glVertex3f( 0.5f, -0.5f, -0.5f);
        glVertex3f( 0.5f,  0.5f, -0.5f); glVertex3f( 0.5f,  0.5f,  0.5f);
        //face -X
        glNormal3f(-1, 0, 0);
        glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f(-0.5f, -0.5f,  0.5f);
        glVertex3f(-0.5f,  0.5f,  0.5f); glVertex3f(-0.5f,  0.5f, -0.5f);
        //face +Y
        glNormal3f(0, 1, 0);
        glVertex3f(-0.5f,  0.5f,  0.5f); glVertex3f( 0.5f,  0.5f,  0.5f);
        glVertex3f( 0.5f,  0.5f, -0.5f); glVertex3f(-0.5f,  0.5f, -0.5f);
        //face -Y
        glNormal3f(0, -1, 0);
        glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f( 0.5f, -0.5f, -0.5f);
        glVertex3f( 0.5f, -0.5f,  0.5f); glVertex3f(-0.5f, -0.5f,  0.5f);
        //face +Z
        glNormal3f(0, 0, 1);
        glVertex3f(-0.5f, -0.5f,  0.5f); glVertex3f( 0.5f, -0.5f,  0.5f);
        glVertex3f( 0.5f,  0.5f,  0.5f); glVertex3f(-0.5f,  0.5f,  0.5f);
        //face -Z
        glNormal3f(0, 0, -1);
        glVertex3f( 0.5f, -0.5f, -0.5f); glVertex3f(-0.5f, -0.5f, -0.5f);
        glVertex3f(-0.5f,  0.5f, -0.5f); glVertex3f( 0.5f,  0.5f, -0.5f);
    glEnd();

    glPopMatrix();
}

//Desenha uma esfera (ou elipsoide) centrada em (cx, cy, cz) com semi-eixos
//(rx, ry, rz).
void desenharElipsoide(float cx, float cy, float cz, float rx, float ry, float rz)
{
    glPushMatrix();
    glTranslatef(cx, cy, cz);
    glScalef(rx, ry, rz);
    gluSphere(quadrica, 1.0, 24, 16);
    glPopMatrix();
}

//Desenha o avião no sistema local, com comprimento total igual a 1:
//  eixo +X = nariz, eixo +Y = cima (up do avião), eixo Z = envergadura.
//A origem é o centro do "corpo do avião";
void desenharAviao()
{
    //corpo do avião
    glColor3f(0.85f, 0.85f, 0.88f);
    desenharElipsoide(0.0f, 0.0f, 0.0f, 0.50f, 0.09f, 0.09f);

    //cabine
    glColor3f(0.20f, 0.35f, 0.60f);
    desenharElipsoide(0.15f, 0.05f, 0.0f, 0.12f, 0.06f, 0.07f);

    //asas, estabilizador horizontal e deriva vertical
    glColor3f(0.85f, 0.15f, 0.15f);
    desenharCaixa( 0.03f, 0.00f, 0.0f, 0.22f, 0.02f, 0.90f);   //asas
    desenharCaixa(-0.42f, 0.00f, 0.0f, 0.12f, 0.02f, 0.34f);   //estabilizador
    desenharCaixa(-0.42f, 0.09f, 0.0f, 0.14f, 0.17f, 0.02f);   //deriva

}

//Aplica a rotação da pose ao avião, em torno do centro da fuselagem.
//Eixos locais: X = nariz, Y = cima, Z = lado direito.
//  subindo / descendo : gira em torno de Z (nariz para cima / para baixo)
//  direita / esquerda : inclina as asas (giro em torno de X) e aponta
//                       um pouco o nariz para o lado da curva (giro em Y)
void aplicarPose(PoseAviao pose)
{
    switch (pose)
    {
        case POSE_SUBINDO:
            glRotatef( ANGULO_PITCH, 0.0f, 0.0f, 1.0f);
            break;
        case POSE_DESCENDO:
            glRotatef(-ANGULO_PITCH, 0.0f, 0.0f, 1.0f);
            break;
        case POSE_DIREITA:
            glRotatef(-ANGULO_YAW,    0.0f, 1.0f, 0.0f);
            glRotatef( ANGULO_ROLL, 1.0f, 0.0f, 0.0f);   //asa direita para baixo
            break;
        case POSE_ESQUERDA:
            glRotatef( ANGULO_YAW,    0.0f, 1.0f, 0.0f);
            glRotatef(-ANGULO_ROLL, 1.0f, 0.0f, 0.0f);   //asa esquerda para baixo
            break;
        default:
            break;
    }
}

void desenhar()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    //Câmera em 3ª pessoa: fica atrás e acima do avião, olhando um pouco à
    //frente dele, com o up da câmera igual ao up do avião. Deve vir ANTES
    //de posicionar a luz, para a posição do Sol ser transformada pela
    //matriz de visão.
    float olho[3], mira[3];
    for (int i = 0; i < 3; i++)
    {
        olho[i] = aviaoPos[i]
                - aviaoFrente[i] * CAMERA_ATRAS  * tamanhoAviao
                + aviaoCima[i]   * CAMERA_ALTURA * tamanhoAviao;
        mira[i] = aviaoPos[i]
                + aviaoFrente[i] * CAMERA_ADIANTE * tamanhoAviao;
    }
    gluLookAt(olho[0], olho[1], olho[2],
              mira[0], mira[1], mira[2],
              aviaoCima[0], aviaoCima[1], aviaoCima[2]);

    //Posição da luz = posição do Sol (w = 1: fonte pontual)
    float posicaoLuz[] = {sol[0], sol[1], sol[2], 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, posicaoLuz);

    //Planeta: centro na origem, iluminado pelo Sol
    glEnable(GL_LIGHTING);
    glColor3f(0.25f, 0.5f, 0.9f);
    gluSphere(quadrica, raio, 192, 96);

    //Avião: posição e orientação vêm do estado do avião. A matriz tem como
    //colunas os eixos locais (X = frente, Y = cima, Z = frente x cima) e a
    //posição, na ordem column-major do OpenGL.
    float eixoZ[3] = {
        aviaoFrente[1] * aviaoCima[2] - aviaoFrente[2] * aviaoCima[1],
        aviaoFrente[2] * aviaoCima[0] - aviaoFrente[0] * aviaoCima[2],
        aviaoFrente[0] * aviaoCima[1] - aviaoFrente[1] * aviaoCima[0]
    };
    //Fora da pose parado, o avião é desenhado no ar (só para visualizar as
    //poses): sobe ALTURA_PREVIA tamanhos de avião ao longo do seu up.
    float elevacao = (poseAviao == POSE_PARADO) ? 0.0f : ALTURA_PREVIA * tamanhoAviao;

    float orientacao[16] = {
        aviaoFrente[0], aviaoFrente[1], aviaoFrente[2], 0.0f,
        aviaoCima[0],   aviaoCima[1],   aviaoCima[2],   0.0f,
        eixoZ[0],       eixoZ[1],       eixoZ[2],       0.0f,
        aviaoPos[0] + aviaoCima[0] * elevacao,
        aviaoPos[1] + aviaoCima[1] * elevacao,
        aviaoPos[2] + aviaoCima[2] * elevacao,
        1.0f
    };

    glPushMatrix();
    glMultMatrixf(orientacao);
    glScalef(tamanhoAviao, tamanhoAviao, tamanhoAviao);
    aplicarPose(poseAviao);
    desenharAviao();
    glPopMatrix();

    //Sol: esfera amarela na posição da luz, sem iluminação
    glDisable(GL_LIGHTING);
    glPushMatrix();
    glTranslatef(sol[0], sol[1], sol[2]);
    glColor3f(1.0f, 0.9f, 0.1f);
    gluSphere(quadrica, raioSol, 48, 24);
    glPopMatrix();

    //troca de buffers
    glfwSwapBuffers(glfwGetCurrentContext());
}

//função chamada quando a tela é redimensionada
void redimensionar(GLFWwindow* janela, int largura, int altura)
{
    if (altura < 1) altura = 1;
    glViewport(0, 0, largura, altura);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    //Projeção em perspectiva, mantendo a proporção da janela
    gluPerspective(FOV, (double)largura / altura,
                   0.2 * tamanhoAviao, planoDistante);

    glMatrixMode(GL_MODELVIEW);
}

//função chamada quando há interação com o teclado
void teclar(GLFWwindow* janela, int tecla, int codigo, int acao, int modificadores)
{
    if (acao != GLFW_PRESS)
        return;

    if (tecla == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(janela, GLFW_TRUE);

    if (tecla == GLFW_KEY_R)
    {
        malha = !malha;
        glPolygonMode(GL_FRONT_AND_BACK, malha ? GL_LINE : GL_FILL);
    }

    //teclas TEMPORÁRIAS para visualizar cada pose; nas próximas etapas a
    //pose passará a ser definida pelos comandos de voo (setas, decolar...)
    if (tecla == GLFW_KEY_1) poseAviao = POSE_PARADO;
    if (tecla == GLFW_KEY_2) poseAviao = POSE_SUBINDO;
    if (tecla == GLFW_KEY_3) poseAviao = POSE_DESCENDO;
    if (tecla == GLFW_KEY_4) poseAviao = POSE_DIREITA;
    if (tecla == GLFW_KEY_5) poseAviao = POSE_ESQUERDA;
}

int main(int argc, char** argv)
{
    if (!obterParametros(argc, argv))
        return EXIT_FAILURE;

    if (!glfwInit())
        return -1;

    GLFWwindow* janela = glfwCreateWindow(1024, 768, "Planeta, Sol e Aviao - Etapa 2", NULL, NULL);

    if (!janela) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(janela);

    //sincroniza a troca de buffers com a atualização do monitor (V-Sync):
    //limita o loop à taxa do monitor e evita processamento desnecessário
    glfwSwapInterval(1);

    //funções orientadas a eventos de entrada
    glfwSetFramebufferSizeCallback(janela, redimensionar);
    glfwSetKeyCallback(janela, teclar);

    glEnable(GL_DEPTH_TEST);

    iniciar();

    //configura a projeção inicial (o callback só roda quando a janela muda)
    int largura, altura;
    glfwGetFramebufferSize(janela, &largura, &altura);
    redimensionar(janela, largura, altura);

    std::cout << "Controles: R = alternar solido/malha | ESC = sair\n"
              << "Poses (temporario): 1 = parado | 2 = subindo | 3 = descendo"
              << " | 4 = direita | 5 = esquerda\n";

    //limitador de quadros por software: garante o ritmo mesmo quando o
    //vsync é ignorado pelo ambiente (WSLg). Cada quadro tem um
    //horário agendado; o loop dorme até ele, liberando a CPU.
    using Relogio = std::chrono::steady_clock;
    const auto periodoQuadro = std::chrono::duration_cast<Relogio::duration>(
        std::chrono::duration<double>(1.0 / FPS_MAXIMO));
    auto proximoQuadro = Relogio::now();

    while (!glfwWindowShouldClose(janela))
    {
        //contador de FPS (temporário, para teste)
        static double ultimoTempo = glfwGetTime();
        static int quadros = 0;
        quadros++;
        double agora = glfwGetTime();
        if (agora - ultimoTempo >= 1.0)
        {
            char titulo[64];
            std::snprintf(titulo, sizeof(titulo), "Planeta, Sol e Aviao - Etapa 2 | %d FPS", quadros);
            glfwSetWindowTitle(janela, titulo);
            quadros = 0;
            ultimoTempo = agora;
        }

        desenhar();

        glfwPollEvents();

        //agenda o próximo quadro e dorme até lá
        proximoQuadro += periodoQuadro;
        auto agoraRelogio = Relogio::now();
        if (proximoQuadro > agoraRelogio)
            std::this_thread::sleep_until(proximoQuadro);
        else
            proximoQuadro = agoraRelogio;   //quadro atrasou: reinicia a agenda, sem rajada
    }

    gluDeleteQuadric(quadrica);
    glfwDestroyWindow(janela);
    glfwTerminate();

    return EXIT_SUCCESS;
}
