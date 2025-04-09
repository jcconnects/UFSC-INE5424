CXX = g++
CXXFLAGS = -std=c++14 -Wall -I./include -g
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
	make setup_dummy_iface
	sudo ./$< $(ARGS)
	make clean_iface

# Limpeza
clean:
	rm -rf $(BINDIR)

setup_dummy_iface:
	@if ! ip link show eth0 > /dev/null 2>&1; then \
		echo "Criando interface dummy eth0..."; \
		sudo ip link add eth0 type dummy; \
		sudo ip link set eth0 up; \
	else \
		echo "Interface dummy eth0 já existe."; \
	fi

clean_iface:
	@if ip link show eth0 > /dev/null 2>&1; then \
		echo "Removendo interface dummy eth0..."; \
		sudo ip link delete eth0 type dummy; \
	fi

.PHONY: all clean dirs run_% setup_dummy_iface clean_iface

