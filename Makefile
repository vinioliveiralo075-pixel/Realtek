# Descobre o caminho do SDK do macOS dinamicamente no servidor
SDKPATH = $(shell xcrun --show-sdk-path)
KERNEL_HEADERS = $(SDKPATH)/System/Library/Frameworks/Kernel.framework/Headers

CXX = clang++

# Flags corrigidas: -DKERNEL e -DAPPLE ativam o modo Kext clássica no SDK da Apple
CXXFLAGS = -mkernel \
           -DKERNEL \
           -DAPPLE \
           -fno-builtin \
           -fno-rtti \
           -fno-exceptions \
           -Wall \
           -std=c++17 \
           -g \
           -isysroot $(SDKPATH) \
           -I$(KERNEL_HEADERS) \
           -I.

# Pega automaticamente RTL8723BE_Driver.cpp e RTL8723BE.cpp
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
