CXX = g++
CXXFLAGS = -std=c++14 -Wall -I./include
LDFLAGS = -pthread

# Diretórios
SRCDIR = src
TESTDIR = tests
BINDIR = bin

# Todos os testes encontrados
TEST_SRCS := $(wildcard $(TESTDIR)/*.cpp)
TEST_BINS := $(patsubst $(TESTDIR)/%.cpp, $(BINDIR)/%, $(TEST_SRCS))

# Fontes principais
SRCS := $(wildcard $(SRCDIR)/*.cpp) $(wildcard $(SRCDIR)/core/*.cpp)

# Alvo padrão: compilar todos os testes
all: dirs $(TEST_BINS)

# Criar diretórios necessários
dirs:
	mkdir -p $(BINDIR)

tests/%: dirs $(BINDIR)/%
	@true

# Regra genérica para compilar cada teste
$(BINDIR)/%: $(TESTDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

# Executar teste específico
run_%: $(BINDIR)/%
	./$<

# Limpeza
clean:
	rm -rf $(BINDIR)

.PHONY: all clean dirs run_%
