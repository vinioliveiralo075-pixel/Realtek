SDKPATH = $(shell xcrun --show-sdk-path)
KERNEL_HEADERS = $(SDKPATH)/System/Library/Frameworks/Kernel.framework/Headers

CXX = clang++

CXXFLAGS = -mkernel \
           -DKERNEL \
           -DAPPLE \
           -fno-builtin \
           -fno-rtti \
           -fno-exceptions \
           -Wall \
           -Wno-inconsistent-missing-override \
           -Wno-deprecated-declarations \
           -std=c++17 \
           -g \
           -isysroot $(SDKPATH) \
           -I$(KERNEL_HEADERS) \
           -I.

# Flags necessárias para o Linker do Kernel do macOS juntar os arquivos .o
LDFLAGS = -Xlinker -kext -nostdlib -lkmodc++ -lkmod -lcc_kext

SRCS = RTL8723BE.cpp RTL8723BE_Driver.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = RTL8723BE

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJS) -o $(TARGET)
	@echo "========================================================================="
	@echo "Boa, Vini! Executável final da Kext gerado com sucesso!"
	@echo "========================================================================="

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGET)
