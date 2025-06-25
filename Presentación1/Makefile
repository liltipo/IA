CXX = g++
CXXFLAGS = -std=c++11 -O2
TARGET = vrpb
SRC = main.cpp
INSTANCES_DIR = Instancias
OUTS_DIR = Outs

INSTANCES = $(wildcard $(INSTANCES_DIR)/G*.txt)
OUTPUTS = $(patsubst $(INSTANCES_DIR)/%.txt,$(OUTS_DIR)/%.out,$(INSTANCES))

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET) $(OUTPUTS)

# Ejecuta el programa para cada instancia y guarda el .out en Outs/
$(OUTS_DIR)/%.out: $(INSTANCES_DIR)/%.txt
	@echo "Procesando $<..."
	@./$(TARGET) $< > /dev/null

clean:
	rm -f $(TARGET)
	rm -f $(OUTS_DIR)/*.out
