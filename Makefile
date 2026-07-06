# Mapeia dinamicamente os caminhos do SDK do Mac no servidor do GitHub
SDKPATH = $(shell xcrun --show-sdk-path)
KERNEL_HEADERS = $(SDKPATH)/System/Library/Frameworks/Kernel.framework/Headers

CXX = clang++

# Flags oficiais para o Kernel, silenciando avisos internos do SDK da Apple
CXXFLAGS = -mkernel \
           -DKERNEL \
           -DAPPLE \
           -fno-builtin \
           -fno-rtti \
           -fno-exceptions \
           -Wall \
           -Wno-inconsistent-missing-override \
           -std=c++17 \
           -g \
           -isysroot $(SDKPATH) \
           -I$(KERNEL_HEADERS) \
           -I.

SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)

all: $(OBJS)
	@echo "========================================================================="
	@echo "Boa, Vini! O GitHub Actions validou a sintaxe de todos os arquivos .cpp!"
	@echo "========================================================================="

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o
