# Compilador padrão no macOS (Clang)
CXX = clang++

# Flags de compilação para C++17
CXXFLAGS = -Wall -std=c++17 -g

# Nome do binário de teste (no Mac não usamos a extensão .exe)
TARGET = teste_driver

# Deteta automaticamente todos os ficheiros .cpp da tua pasta
SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Comando de limpeza universal para Mac e Linux
clean:
	rm -f $(TARGET) *.o