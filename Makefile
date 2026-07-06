# Descobre o caminho do SDK do macOS dinamicamente no servidor do GitHub
SDKPATH = $(shell xcrun --show-sdk-path)
KERNEL_HEADERS = $(SDKPATH)/System/Library/Frameworks/Kernel.framework/Headers

CXX = clang++

# Flags obrigatórias para desenvolvimento de Kernel (Kext) no Mac
CXXFLAGS = -mkernel \
           -fno-builtin \
           -fno-rtti \
           -fno-exceptions \
           -Wall \
           -std=c++17 \
           -g \
           -isysroot $(SDKPATH) \
           -I$(KERNEL_HEADERS) \
           -I.

# Pega todos os arquivos .cpp da sua pasta (RTL8723BE_Driver.cpp e RTL8723BE.cpp)
SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)

# Alvo principal: compila os objetos para validar a sintaxe do código
all: $(OBJS)
	@echo "========================================================================="
	@echo "Boa, Vini! O GitHub Actions validou a sintaxe de todos os arquivos .cpp!"
	@echo "========================================================================="

# Regra para compilar cada arquivo .cpp em um objeto .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o
