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

# Alvo padrão: compilar e executar todos os testes
.PHONY: all
all: test

# Compilar e executar todos os testes
.PHONY: test
test: dirs $(TEST_BINS) run_all_tests

# Alvo para compilar todos os testes
.PHONY: compile_tests
compile_tests: dirs $(TEST_BINS)

# Criar diretórios necessários
dirs:
	mkdir -p $(BINDIR)

tests/%: dirs $(BINDIR)/%
	@true

# Regra genérica para compilar cada teste
$(BINDIR)/%: $(TESTDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

# Executar todos os testes
.PHONY: run_all_tests
run_all_tests: setup_dummy_iface
	@echo "Running all tests..."
	@for test in $(TEST_BINS); do \
		echo "\n=== Running test $$test ==="; \
		sudo ./$$test $(ARGS); \
		if [ $$? -ne 0 ]; then \
			echo "Test $$test failed!"; \
			make clean_iface; \
			exit 1; \
		fi; \
	done
	@make clean_iface
	@echo "All tests completed successfully!"

# Executar teste específico
.PHONY: run_%
run_%: $(BINDIR)/%
	make setup_dummy_iface
	sudo ./$< $(ARGS)
	make clean_iface

# Limpeza
.PHONY: clean
clean:
	rm -rf $(BINDIR)

.PHONY: setup_dummy_iface
setup_dummy_iface:
	@if ! ip link show eth0 > /dev/null 2>&1; then \
		echo "Criando interface dummy eth0..."; \
		sudo ip link add eth0 type dummy; \
		sudo ip link set eth0 up; \
	else \
		echo "Interface dummy eth0 já existe."; \
	fi

.PHONY: clean_iface
clean_iface:
	@if ip link show eth0 > /dev/null 2>&1; then \
		echo "Removendo interface dummy eth0..."; \
		sudo ip link delete eth0 type dummy; \
	fi

.PHONY: dirs

