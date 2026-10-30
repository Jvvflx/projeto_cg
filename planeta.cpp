// Etapa 1: planeta (esfera na origem) + Sol (esfera amarela na posição da luz)
// Tecla R: alterna entre modo sólido e malha (wireframe). ESC: sair.
//
// Compilar no Linux:
// g++ -o planeta planeta.cpp -lglfw -lGL -lGLU
//
// Uso:  ./planeta <raio> <solX> <solY> <solZ>
// Ex.:  ./planeta 1.0 5 2 3
// Sem argumentos, os valores são lidos da entrada padrão.

#include <GL/gl.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

float raio = 1.0f;
float sol[3] = {5.0f, 2.0f, 3.0f};
float raioSol;            // tamanho do Sol, proporcional ao planeta
float distanciaCamera;    // afastamento da câmera
float alvo[3];            // ponto para onde a câmera olha
float extensao;           // tamanho da cena, usado para enquadrar

bool malha = false;
const float FOV = 45.0f;

GLUquadric* quadrica;

void iniciar()
{
   //define a cor de limpeza
   glClearColor(0.02, 0.02, 0.05, 1.0);

   //modo de desenho dos polígonos
   glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

   //modo de sombreamento
   glShadeModel(GL_SMOOTH);

   //Calcula o enquadramento: a câmera olha para o ponto médio entre
   //o planeta e o Sol, afastada o bastante para enquadrar os dois.
   float distSol = std::sqrt(sol[0]*sol[0] + sol[1]*sol[1] + sol[2]*sol[2]);
   raioSol = 0.25f * raio;

   alvo[0] = sol[0] * 0.5f;
   alvo[1] = sol[1] * 0.5f;
   alvo[2] = sol[2] * 0.5f;

   extensao = std::max(raio, distSol * 0.5f + raioSol + raio);
   distanciaCamera = 1.4f * extensao / std::tan(FOV * 0.5f * M_PI / 180.0f);

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

   //objeto usado por gluSphere
   quadrica = gluNewQuadric();
   gluQuadricNormals(quadrica, GLU_SMOOTH);
}

void desenhar()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    //Câmera: deve vir ANTES de posicionar a luz, para a posição
    //do Sol ser transformada pela matriz de visão.
    gluLookAt(alvo[0] + 0.28f * distanciaCamera,
              alvo[1] + 0.47f * distanciaCamera,
              alvo[2] + 0.84f * distanciaCamera,
              alvo[0], alvo[1], alvo[2],
              0.0, 1.0, 0.0);

    //Posição da luz = posição do Sol (w = 1: fonte pontual)
    float posicaoLuz[] = {sol[0], sol[1], sol[2], 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, posicaoLuz);

    //Planeta: centro na origem, iluminado pelo Sol
    glEnable(GL_LIGHTING);
    glColor3f(0.25f, 0.5f, 0.9f);
    gluSphere(quadrica, raio, 96, 48);

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
                   0.1, distanciaCamera + extensao * 4.0f);

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
}

int main(int argc, char** argv)
{
    if (argc >= 5)
    {
        raio   = std::strtof(argv[1], NULL);
        sol[0] = std::strtof(argv[2], NULL);
        sol[1] = std::strtof(argv[3], NULL);
        sol[2] = std::strtof(argv[4], NULL);
    }
    else
    {
        std::cout << "Raio do planeta: ";
        std::cin >> raio;
        std::cout << "Posicao do Sol (x y z): ";
        std::cin >> sol[0] >> sol[1] >> sol[2];
    }

    if (raio <= 0.0f)
    {
        std::cerr << "O raio deve ser positivo.\n";
        return EXIT_FAILURE;
    }

    float distSol = std::sqrt(sol[0]*sol[0] + sol[1]*sol[1] + sol[2]*sol[2]);
    if (distSol <= raio)
        std::cerr << "Aviso: o Sol esta dentro do planeta; nao sera visivel.\n";

    if (!glfwInit())
        return -1;

    GLFWwindow* janela = glfwCreateWindow(1024, 768, "Planeta e Sol - Etapa 1", NULL, NULL);

    if (!janela) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(janela);

    //funções orientadas a eventos de entrada
    glfwSetFramebufferSizeCallback(janela, redimensionar);
    glfwSetKeyCallback(janela, teclar);

    glEnable(GL_DEPTH_TEST);

    iniciar();

    //configura a projeção inicial (o callback só roda quando a janela muda)
    int largura, altura;
    glfwGetFramebufferSize(janela, &largura, &altura);
    redimensionar(janela, largura, altura);

    std::cout << "Controles: R = alternar solido/malha | ESC = sair\n";

    while (!glfwWindowShouldClose(janela))
    {
        desenhar();

        glfwPollEvents();
    }

    gluDeleteQuadric(quadrica);
    glfwDestroyWindow(janela);
    glfwTerminate();

    return EXIT_SUCCESS;
}