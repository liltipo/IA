CXX = g++
CXXFLAGS = -std=c++11 -O2

SRC_DIR = src
INSTANCES_DIR = Instancias
OUTS_DIR = Outs

VERSIONS = v1 v2 v3 v4 v5
SOURCES = $(addprefix $(SRC_DIR)/, $(addsuffix .cpp, $(VERSIONS)))
BINARIES = $(addprefix bin/, $(addprefix vrpb_, $(VERSIONS)))

INSTANCES = $(wildcard $(INSTANCES_DIR)/G*.txt)

.PHONY: all run clean

# Compila todos los ejecutables
all: $(BINARIES)

# Compila cada ejecutable
bin/vrpb_%: $(SRC_DIR)/%.cpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $@ $<

# Ejecuta todas las versiones en todas las instancias
run: all
	@for v in $(VERSIONS); do \
		mkdir -p $(OUTS_DIR)/$$v; \
		for f in $(INSTANCES); do \
			name=$$(basename $$f .txt); \
			echo "Ejecutando $$f con versión $$v..."; \
			./bin/vrpb_$$v $$f $(OUTS_DIR)/$$v/$$name.out; \
		done \
	done

# Limpia binarios y salidas
clean:
	rm -rf bin
	rm -rf $(OUTS_DIR)
