
# Compilador
CXX = g++

# Flags de compilação
CXXFLAGS = -Wall -Wextra -std=c++17

# Bibliotecas OpenGL e GLFW
LIBS = -lglfw -lGL -lGLU

# Nome do executável
TARGET = planeta

# Arquivo fonte
SOURCE = planeta.cpp

# Regra padrão
all: $(TARGET)

# Compilação
$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) $(SOURCE) -o $(TARGET) $(LIBS)

# Executar com valores padrão (entrada via terminal)
run: $(TARGET)
	./$(TARGET)

# Executar com parâmetros
run-args: $(TARGET)
	./$(TARGET) 1.0 5 2 3

# Limpar arquivos gerados
clean:
	rm -f $(TARGET)

# Recompilar do zero
rebuild: clean all

.PHONY: all run run-args clean rebuild
