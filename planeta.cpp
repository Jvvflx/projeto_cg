// Etapa 2: o aviao sobrevoa o planeta, com camera em 1ª pessoa.
// Controles: D = decolar | P = pousar | setas cima/baixo = velocidade
//            setas laterais = girar | C = camera 1ª/3ª pessoa
//            R = solido/malha | ESC = sair
//
// Compilar no Linux:
// g++ -o planeta planeta.cpp -lglfw -lGL -lGLU
//
// Uso:  ./planeta <raio> <solX> <solY> <solZ>
// Ex.:  ./planeta 1.0 5 2 3
// Sem argumentos, os valores sao lidos da entrada padrao.

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
float tamanhoAviao;       // comprimento do aviao, proporcional ao planeta
float planoDistante;      // limite do alcance de visao (plano far)
float planoProximo;       // inicio do alcance de visao (plano near)

bool malha = false;
bool terceiraPessoa = false;   // false: camera na cabine (pedido do enunciado)

const float PI = 3.14159265358979f;
const float FOV = 45.0f;

const double FPS_MAXIMO = 60.0;   // limite de quadros por segundo (limitador por software)

const float PROPORCAO_SOL   = 0.25f;  // raio do Sol em relacao ao raio do planeta
const float PROPORCAO_AVIAO = 0.10f;  // comprimento do aviao em relacao ao raio do planeta
const float LIMITE_VALOR    = 1.0e6f; // maior valor absoluto aceito na entrada

// ---------------------------------------------------------------------------
// Parametros de voo
// Distancias e velocidades sao proporcionais ao raio do planeta, para o
// simulador se comportar igual com qualquer raio dado na entrada.
// ---------------------------------------------------------------------------

const float PROPORCAO_ALTURA_VOO = 0.25f; // h: altura de voo, em raios do planeta
const float ALTURA_SOLO          = 0.10f; // centro do aviao pousado, em tamanhos de aviao
                                          // (o corpo tem raio 0.09: a barriga encosta no chao)
const float ALTURA_OLHO          = 0.08f; // olho do piloto acima do centro do aviao

const float VEL_INICIAL = 0.30f;   // velocidade apos decolar, em raios por segundo
const float VEL_MINIMA  = 0.05f;
const float VEL_MAXIMA  = 1.50f;
const float VEL_PASSO   = 0.05f;   // variacao a cada toque nas setas cima/baixo
const float VEL_GIRO    = 60.0f;   // velocidade de giro nas setas laterais (graus/s)

const float DURACAO_DECOLAGEM = 3.0f;  // segundos
const float DURACAO_POUSO     = 3.0f;

//"olhar um pouco para baixo" (nota do enunciado): no chao a camera olha
//quase reto; em voo, inclina mais para manter o solo do planeta na tela
const float INCLINACAO_SOLO = 10.0f;   // graus
const float INCLINACAO_VOO  = 40.0f;

const float DT_MAXIMO = 0.1f;  // maior passo de tempo simulado por quadro (s)

//camera em 3ª pessoa, em multiplos do comprimento do aviao
const float CAMERA_ATRAS   = 3.0f;    // quanto fica atras do aviao
const float CAMERA_ALTURA  = 1.2f;    // quanto fica acima do aviao
const float CAMERA_ADIANTE = 2.0f;    // ponto mirado, à frente do aviao

// ---------------------------------------------------------------------------
// Estado do aviao
// ---------------------------------------------------------------------------

//maquina de estados do voo
enum EstadoVoo { NO_CHAO, DECOLANDO, VOANDO, POUSANDO };
const char* NOME_ESTADO[] = {"no chao", "decolando", "voando", "pousando"};
EstadoVoo estado = NO_CHAO;

//Orientacao do aviao: base ortonormal (vetores unitarios e perpendiculares).
//  aviaoCima   = normal do planeta no ponto onde o aviao esta (up)
//  aviaoFrente = direcao do nariz, sempre tangente ao planeta
//A posicao e derivada: aviaoPos = aviaoCima * (raio + altitude).
float aviaoCima[3]   = {0.0f, 1.0f, 0.0f};
float aviaoFrente[3] = {1.0f, 0.0f, 0.0f};
float aviaoPos[3];

float altitude;        // altura atual do centro do aviao acima do solo
float alturaSolo;      // altitude quando pousado
float alturaVoo;       // altitude de cruzeiro (h)
float velocidade = 0;  // em unidades do mundo por segundo

//decolagem e pouso: progresso da manobra (0 a 1) e valores no inicio dela
float progresso = 0.0f;
float altitudeInicial, velocidadeInicial;

int sentidoGiro = 0;   // +1 esquerda, -1 direita, 0 reto (setas laterais)

//poses do aviao
enum PoseAviao { POSE_PARADO, POSE_SUBINDO, POSE_DESCENDO, POSE_DIREITA, POSE_ESQUERDA };

const float ANGULO_PITCH     = 20.0f;  // inclinacao do nariz ao subir/descer (graus)
const float ANGULO_ROLL = 30.0f;  // inclinacao lateral das asas nas curvas (graus)
const float ANGULO_YAW    = 10.0f;  // quanto o nariz aponta para o lado da curva (graus)

GLUquadric* quadrica;

//Converte o texto em exatamente n numeros reais finitos.
//Retorna false se faltar numero, sobrar caractere, houver estouro
//de faixa ou valor nao finito (nan, inf).
bool lerNumeros(const char* texto, float* valores, int n)
{
    const char* p = texto;

    for (int i = 0; i < n; i++)
    {
        char* fim;
        errno = 0;
        float v = std::strtof(p, &fim);

        //fim == p: nada foi convertido; ERANGE: nao cabe em float
        if (fim == p || errno == ERANGE || !std::isfinite(v))
            return false;

        valores[i] = v;
        p = fim;
    }

    //depois dos n numeros so podem sobrar espacos
    while (std::isspace((unsigned char)*p))
        p++;

    return *p == '\0';
}

//Verifica se os parametros formam uma cena valida.
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

    //o Sol nao pode tocar o planeta: distancia entre os centros
    //tem de ser maior que a soma dos raios
    float distSol = std::sqrt(posSol[0]*posSol[0] + posSol[1]*posSol[1] + posSol[2]*posSol[2]);
    if (distSol <= raioPlaneta * (1.0f + PROPORCAO_SOL))
        return "o Sol esta dentro ou encostado no planeta; afaste-o do centro.";

    //o aviao voa a uma distancia raio + h do centro: o Sol nao pode
    //alcancar essa altura, senao o aviao atravessaria o Sol
    float alcanceAviao = raioPlaneta * (1.0f + PROPORCAO_ALTURA_VOO + PROPORCAO_AVIAO);
    if (distSol <= alcanceAviao + raioPlaneta * PROPORCAO_SOL)
        return "o Sol esta na altura de voo do aviao; afaste-o do centro.";
    return NULL;
}

void mostrarUso(const char* programa)
{
    std::cerr << "Uso: " << programa << " <raio> <solX> <solY> <solZ>\n"
              << "  Ex.: " << programa << " 1.0 5 2 3\n"
              << "  Sem argumentos, os valores sao pedidos pelo teclado.\n"
              << "  Use ponto como separador decimal (1.5, nao 1,5).\n";
}

//Le uma linha do teclado e converte em n numeros, repetindo ate a
//entrada ser valida. Retorna false se a entrada terminar (Ctrl+D).
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

//Obtem raio e posicao do Sol pelos argumentos ou pelo teclado.
//Retorna false se nao for possivel obter uma cena valida.
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

    //qualquer quantidade diferente de 0 ou 4 argumentos e erro
    if (argc != 1)
    {
        std::cerr << "Erro: numero de argumentos incorreto.\n";
        mostrarUso(argv[0]);
        return false;
    }

    //modo interativo: pergunta ate obter uma cena valida
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

// ---------------------------------------------------------------------------
// Tratamento de erros
// ---------------------------------------------------------------------------

//Chamada pelo GLFW sempre que ocorre um erro, com o codigo e a descricao.
//Registrada antes do glfwInit para capturar tambem falhas na inicializacao.
void erroGLFW(int codigo, const char* descricao)
{
    std::cerr << "Erro GLFW (0x" << std::hex << codigo << std::dec << "): "
              << descricao << "\n";
}

//Esvazia a fila de erros do OpenGL, imprimindo cada um.
//O OpenGL nao avisa quando uma chamada falha: guarda uma flag de erro
//que so e lida com glGetError(). Retorna true se houve algum erro.
bool verificarErrosGL(const char* local)
{
    bool houveErro = false;
    GLenum erro;

    while ((erro = glGetError()) != GL_NO_ERROR)
    {
        std::cerr << "Erro OpenGL em " << local << ": "
                  << (const char*)gluErrorString(erro) << "\n";
        houveErro = true;
    }
    return houveErro;
}

// ---------------------------------------------------------------------------
// Operacoes com vetores 3D
// ---------------------------------------------------------------------------

float produtoEscalar(const float a[3], const float b[3])
{
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

void produtoVetorial(const float a[3], const float b[3], float r[3])
{
    r[0] = a[1]*b[2] - a[2]*b[1];
    r[1] = a[2]*b[0] - a[0]*b[2];
    r[2] = a[0]*b[1] - a[1]*b[0];
}

void normalizar(float v[3])
{
    float comprimento = std::sqrt(produtoEscalar(v, v));
    if (comprimento > 0.0f)
        for (int i = 0; i < 3; i++)
            v[i] /= comprimento;
}

//Gira v em torno do eixo unitario k por um angulo (radianos), pela
//formula de Rodrigues -- a mesma rotacao que glRotatef aplica a uma matriz:
//  v' = v cos(a) + (k x v) sen(a) + k (k . v)(1 - cos(a))
void rotacionar(float v[3], const float k[3], float angulo)
{
    float c = std::cos(angulo), s = std::sin(angulo);
    float kxv[3];
    produtoVetorial(k, v, kxv);
    float kv = produtoEscalar(k, v);

    for (int i = 0; i < 3; i++)
        v[i] = v[i]*c + kxv[i]*s + k[i]*kv*(1.0f - c);
}

// ---------------------------------------------------------------------------
// Voo: comandos e atualizacao
// ---------------------------------------------------------------------------

//Curva suave de 0 a 1 (comeca e termina devagar): smoothstep
float suavizar(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

void decolar()
{
    if (estado != NO_CHAO)
    {
        std::cout << "Decolar: o aviao ja esta no ar.\n";
        return;
    }
    estado = DECOLANDO;
    progresso = 0.0f;
    velocidade = VEL_INICIAL * raio;
}

void pousar()
{
    if (estado == NO_CHAO || estado == POUSANDO)
    {
        std::cout << "Pousar: o aviao ja esta " << NOME_ESTADO[estado] << ".\n";
        return;
    }
    //pode pousar em voo ou no meio da decolagem: parte da altura atual
    estado = POUSANDO;
    progresso = 0.0f;
    altitudeInicial = altitude;
    velocidadeInicial = velocidade;
}

//sinal = +1 acelera, -1 freia; so vale em voo de cruzeiro
void mudarVelocidade(int sinal, bool avisar)
{
    if (estado != VOANDO)
    {
        if (avisar)
            std::cout << "Velocidade: so pode ser alterada em voo (aviao "
                      << NOME_ESTADO[estado] << ").\n";
        return;
    }
    velocidade += sinal * VEL_PASSO * raio;
    velocidade = std::max(VEL_MINIMA * raio, std::min(velocidade, VEL_MAXIMA * raio));
}

//Avanca o voo em dt segundos.
void atualizarVoo(GLFWwindow* janela, float dt)
{
    //1. Altitude e velocidade conforme o estado. Fora da decolagem e do
    //pouso a altitude nao muda: o piloto nao controla a altura.
    if (estado == DECOLANDO)
    {
        progresso = std::min(progresso + dt / DURACAO_DECOLAGEM, 1.0f);
        altitude = alturaSolo + (alturaVoo - alturaSolo) * suavizar(progresso);

        if (progresso >= 1.0f)
            estado = VOANDO;
    }
    else if (estado == POUSANDO)
    {
        progresso = std::min(progresso + dt / DURACAO_POUSO, 1.0f);
        altitude = altitudeInicial + (alturaSolo - altitudeInicial) * suavizar(progresso);
        velocidade = velocidadeInicial * (1.0f - progresso);   //freia ate parar

        if (progresso >= 1.0f)
        {
            estado = NO_CHAO;
            velocidade = 0.0f;
        }
    }

    //2. Giro: enquanto uma seta lateral estiver pressionada, a frente gira
    //em torno do up do aviao. Consulta o estado da tecla a cada quadro
    //(o callback so avisa quando a tecla e apertada ou solta).
    sentidoGiro = 0;
    if (glfwGetKey(janela, GLFW_KEY_LEFT)  == GLFW_PRESS) sentidoGiro += 1;
    if (glfwGetKey(janela, GLFW_KEY_RIGHT) == GLFW_PRESS) sentidoGiro -= 1;

    if (sentidoGiro != 0)
        rotacionar(aviaoFrente, aviaoCima, sentidoGiro * VEL_GIRO * PI / 180.0f * dt);

    //3. Avanco: andar sobre a esfera e girar em torno do centro do planeta.
    //O eixo e perpendicular ao up e à frente (cima x frente); o angulo e o
    //arco percorrido dividido pelo raio da orbita (s = r * theta).
    //Up e frente giram juntos, entao o aviao continua tangente ao planeta.
    float anguloAvanco = velocidade * dt / (raio + altitude);
    if (anguloAvanco > 0.0f)
    {
        float eixo[3];
        produtoVetorial(aviaoCima, aviaoFrente, eixo);
        normalizar(eixo);
        rotacionar(aviaoCima, eixo, anguloAvanco);
        rotacionar(aviaoFrente, eixo, anguloAvanco);
    }

    //4. Corrige o acumulo de erros de arredondamento: mantem os vetores
    //unitarios e perpendiculares (ortogonalizacao de Gram-Schmidt - obrigado prof Gastao)
    normalizar(aviaoCima);
    float projecao = produtoEscalar(aviaoFrente, aviaoCima);
    for (int i = 0; i < 3; i++)
        aviaoFrente[i] -= projecao * aviaoCima[i];
    normalizar(aviaoFrente);

    //5. Posicao: sobre a normal, a uma distancia raio + altitude do centro
    for (int i = 0; i < 3; i++)
        aviaoPos[i] = aviaoCima[i] * (raio + altitude);
}

//A pose do desenho decorre do que o aviao esta fazendo
PoseAviao poseAtual()
{
    if (estado == DECOLANDO) return POSE_SUBINDO;
    if (estado == POUSANDO)  return POSE_DESCENDO;
    if (sentidoGiro > 0)     return POSE_ESQUERDA;
    if (sentidoGiro < 0)     return POSE_DIREITA;
    return POSE_PARADO;
}

bool iniciar()
{
   //define a cor de limpeza
   glClearColor(0.02, 0.02, 0.05, 1.0);

   //modo de desenho dos poligonos
   glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

   //modo de sombreamento
   glShadeModel(GL_SMOOTH);

   //Tamanhos proporcionais ao planeta
   float distSol = std::sqrt(sol[0]*sol[0] + sol[1]*sol[1] + sol[2]*sol[2]);
   raioSol      = PROPORCAO_SOL * raio;
   tamanhoAviao = PROPORCAO_AVIAO * raio;
   alturaSolo   = ALTURA_SOLO * tamanhoAviao;
   alturaVoo    = PROPORCAO_ALTURA_VOO * raio;

   //Aviao pousado no topo do planeta (y), com o nariz para +x
   altitude = alturaSolo;
   for (int i = 0; i < 3; i++)
       aviaoPos[i] = aviaoCima[i] * (raio + altitude);

   //Alcance de visao: o near e pequeno para o chao nao ser cortado com o
   //aviao pousado (olho a poucos centesimos de raio do solo); o far cobre
   //o planeta inteiro e o Sol, de qualquer ponto da orbita
   planoProximo  = 0.05f * tamanhoAviao;
   planoDistante = 2.0f * (distSol + raio + alturaVoo);

   //Iluminacao: o Sol e a fonte de luz (ambiente 0.15 + difusa 0.85)
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

   //as pecas do aviao sao escaladas (glScalef): renormaliza as normais
   //para a iluminacao continuar correta
   glEnable(GL_NORMALIZE);

   //objeto usado por gluSphere
   quadrica = gluNewQuadric();
   if (!quadrica)
   {
       std::cerr << "Erro: memoria insuficiente para criar a quadrica.\n";
       return false;
   }
   gluQuadricNormals(quadrica, GLU_SMOOTH);

   return true;
}

//Desenha um paralelepipedo centrado em (cx, cy, cz) com dimensoes
//(sx, sy, sz). Usado para asas, estabilizador e deriva do aviao.
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

//Desenha o aviao no sistema local, com comprimento total igual a 1:
//  eixo +X = nariz, eixo +Y = cima (up do aviao), eixo Z = envergadura.
//A origem e o centro do "corpo do aviao";
void desenharAviao()
{
    //corpo do aviao
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

//Aplica a rotacao da pose ao aviao, em torno do centro da fuselagem.
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

//Transformacao de visualizacao (gluLookAt), na matriz MODELVIEW.
//gluLookAt recebe o olho (e), um ponto mirado (e + g) e o view-up (t), e
//monta a base da camera: w = -g/|g|, u = t x w (normalizado), v = w x u.
//Por isso o up nao precisa ser perpendicular à direcao do olhar.
void posicionarCamera()
{
    float olho[3], mira[3];

    if (terceiraPessoa)
    {
        //atras e acima do aviao, mirando um pouco à frente dele
        for (int i = 0; i < 3; i++)
        {
            olho[i] = aviaoPos[i]
                    - aviaoFrente[i] * CAMERA_ATRAS  * tamanhoAviao
                    + aviaoCima[i]   * CAMERA_ALTURA * tamanhoAviao;
            mira[i] = aviaoPos[i]
                    + aviaoFrente[i] * CAMERA_ADIANTE * tamanhoAviao;
        }
    }
    else
    {
        //1ª pessoa: o olho esta na cabine; o olhar e a frente inclinada
        //para baixo (rotacao da frente em direcao a -up). A inclinacao
        //cresce com a altitude, para o solo continuar visivel.
        float fracaoAltura = (altitude - alturaSolo) / (alturaVoo - alturaSolo);
        fracaoAltura = std::max(0.0f, std::min(fracaoAltura, 1.0f));

        float inclinacao = (INCLINACAO_SOLO + (INCLINACAO_VOO - INCLINACAO_SOLO) * fracaoAltura)
                         * PI / 180.0f;

        for (int i = 0; i < 3; i++)
        {
            olho[i] = aviaoPos[i] + aviaoCima[i] * ALTURA_OLHO * tamanhoAviao;
            float olhar = aviaoFrente[i] * std::cos(inclinacao)
                        - aviaoCima[i]   * std::sin(inclinacao);
            mira[i] = olho[i] + olhar;
        }
    }

    gluLookAt(olho[0], olho[1], olho[2],
              mira[0], mira[1], mira[2],
              aviaoCima[0], aviaoCima[1], aviaoCima[2]);
}

void desenhar(GLFWwindow* janela)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    //Camera: deve vir ANTES de posicionar a luz, para a posicao do Sol
    //ser transformada pela matriz de visao
    posicionarCamera();

    //Posicao da luz = posicao do Sol (w = 1: fonte pontual)
    float posicaoLuz[] = {sol[0], sol[1], sol[2], 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, posicaoLuz);

    //Planeta: centro na origem, iluminado pelo Sol
    glEnable(GL_LIGHTING);
    glColor3f(0.25f, 0.5f, 0.9f);
    gluSphere(quadrica, raio, 192, 96);

    //Aviao: so aparece em 3ª pessoa (em 1ª pessoa o olho esta dentro dele).
    //A matriz de orientacao muda do sistema local do aviao para o do mundo:
    //as colunas sao os eixos locais (X = frente, Y = cima, Z = frente x cima)
    //e a posicao, na ordem column-major do OpenGL. Os eixos sao unitarios e
    //perpendiculares, entao a matriz so gira e translada (nao deforma).
    if (terceiraPessoa)
    {
        float eixoZ[3];
        produtoVetorial(aviaoFrente, aviaoCima, eixoZ);

        float orientacao[16] = {
            aviaoFrente[0], aviaoFrente[1], aviaoFrente[2], 0.0f,
            aviaoCima[0],   aviaoCima[1],   aviaoCima[2],   0.0f,
            eixoZ[0],       eixoZ[1],       eixoZ[2],       0.0f,
            aviaoPos[0],    aviaoPos[1],    aviaoPos[2],    1.0f
        };

        glPushMatrix();
        glMultMatrixf(orientacao);
        glScalef(tamanhoAviao, tamanhoAviao, tamanhoAviao);
        aplicarPose(poseAtual());
        desenharAviao();
        glPopMatrix();
    }

    //Sol: esfera amarela na posicao da luz, sem iluminacao
    glDisable(GL_LIGHTING);
    glPushMatrix();
    glTranslatef(sol[0], sol[1], sol[2]);
    glColor3f(1.0f, 0.9f, 0.1f);
    gluSphere(quadrica, raioSol, 48, 24);
    glPopMatrix();

    //troca de buffers
    glfwSwapBuffers(janela);
}

//funcao chamada quando a tela e redimensionada
void redimensionar(GLFWwindow* /*janela*/, int largura, int altura)
{
    if (altura < 1) altura = 1;
    glViewport(0, 0, largura, altura);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    //Projecao em perspectiva, mantendo a proporcao da janela
    gluPerspective(FOV, (double)largura / altura, planoProximo, planoDistante);

    glMatrixMode(GL_MODELVIEW);
}

//funcao chamada quando ha interacao com o teclado
//(as setas laterais sao lidas em atualizarVoo, por serem continuas)
void teclar(GLFWwindow* janela, int tecla, int /*codigo*/, int acao, int /*modificadores*/)
{
    if (acao == GLFW_RELEASE)
        return;

    //GLFW_REPEAT: tecla mantida pressionada. So as setas de velocidade
    //repetem; os outros comandos valem uma vez por toque.
    bool repeticao = (acao == GLFW_REPEAT);

    switch (tecla)
    {
        case GLFW_KEY_UP:   mudarVelocidade(+1, !repeticao); return;
        case GLFW_KEY_DOWN: mudarVelocidade(-1, !repeticao); return;
        default: break;
    }

    if (repeticao)
        return;

    switch (tecla)
    {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(janela, GLFW_TRUE);
            break;
        case GLFW_KEY_R:
            malha = !malha;
            glPolygonMode(GL_FRONT_AND_BACK, malha ? GL_LINE : GL_FILL);
            break;
        case GLFW_KEY_C:
            terceiraPessoa = !terceiraPessoa;
            break;
        case GLFW_KEY_D:
            decolar();
            break;
        case GLFW_KEY_P:
            pousar();
            break;
        default:
            break;
    }
}

//Mostra no titulo da janela o FPS e o estado do voo
void atualizarTitulo(GLFWwindow* janela, int fps)
{
    char titulo[128];
    std::snprintf(titulo, sizeof(titulo),
                  "Etapa 2 | %s | vel %.2f raios/s | camera %s | %d FPS",
                  NOME_ESTADO[estado], velocidade / raio,
                  terceiraPessoa ? "3a pessoa" : "1a pessoa", fps);
    glfwSetWindowTitle(janela, titulo);
}

int main(int argc, char** argv)
{
    if (!obterParametros(argc, argv))
        return EXIT_FAILURE;

    //registrado antes do glfwInit para capturar tambem falhas na inicializacao
    glfwSetErrorCallback(erroGLFW);

    if (!glfwInit())
    {
        std::cerr << "Erro: nao foi possivel inicializar o GLFW.\n"
                  << "  Verifique se ha ambiente grafico disponivel"
                  << " (no WSL: 'wsl --update' e 'echo $DISPLAY').\n";
        return EXIT_FAILURE;
    }

    GLFWwindow* janela = glfwCreateWindow(1024, 768, "Etapa 2", NULL, NULL);

    if (!janela)
    {
        std::cerr << "Erro: nao foi possivel criar a janela OpenGL.\n"
                  << "  Verifique o driver de video ('glxinfo -B').\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(janela);

    //sincroniza a troca de buffers com a atualizacao do monitor (V-Sync):
    //limita o loop à taxa do monitor e evita processamento desnecessario
    glfwSwapInterval(1);

    //funcoes orientadas a eventos de entrada
    glfwSetFramebufferSizeCallback(janela, redimensionar);
    glfwSetKeyCallback(janela, teclar);

    glEnable(GL_DEPTH_TEST);

    if (!iniciar())
    {
        glfwDestroyWindow(janela);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    //configura a projecao inicial (o callback so roda quando a janela muda)
    int largura, altura;
    glfwGetFramebufferSize(janela, &largura, &altura);
    redimensionar(janela, largura, altura);

    //qualquer chamada OpenGL invalida ate aqui e reportada
    verificarErrosGL("inicializacao");

    std::cout << "Controles:\n"
              << "  D = decolar | P = pousar\n"
              << "  Setas cima/baixo = velocidade | Setas laterais = girar\n"
              << "  C = camera 1a/3a pessoa | R = solido/malha | ESC = sair\n";

    //limitador de quadros por software: garante o ritmo mesmo quando o
    //vsync e ignorado pelo ambiente (WSLg). Cada quadro tem um
    //horario agendado; o loop dorme ate ele, liberando a CPU.
    using Relogio = std::chrono::steady_clock;
    const auto periodoQuadro = std::chrono::duration_cast<Relogio::duration>(
        std::chrono::duration<double>(1.0 / FPS_MAXIMO));
    auto proximoQuadro = Relogio::now();

    double tempoAnterior = glfwGetTime();
    double inicioContagem = tempoAnterior;
    int quadros = 0;

    while (!glfwWindowShouldClose(janela))
    {
        //Tempo decorrido desde o quadro anterior: o movimento depende do
        //tempo, nao do numero de quadros, entao a velocidade do aviao e a
        //mesma em qualquer computador. O dt e limitado para um travamento
        //(ex.: janela arrastada) nao virar um salto enorme do aviao.
        double agora = glfwGetTime();
        float dt = std::min((float)(agora - tempoAnterior), DT_MAXIMO);
        tempoAnterior = agora;

        glfwPollEvents();
        atualizarVoo(janela, dt);
        desenhar(janela);

        //FPS e estado no titulo, uma vez por segundo
        quadros++;
        if (agora - inicioContagem >= 1.0)
        {
            atualizarTitulo(janela, quadros);
            quadros = 0;
            inicioContagem = agora;
        }

        //agenda o proximo quadro e dorme ate la
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
