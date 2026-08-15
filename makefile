CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -std=c11 -Iinclude
LDLIBS   ?= -lm
PYTHON     ?= python3
PIP_FLAGS  ?=

SRC_DIR      := src
SCRIPTS_DIR  := scripts
BUILD_DIR    := build
RESULTS_DIR  := results
PLOTS_DIR    := $(RESULTS_DIR)/plots

CSV_PATH   := $(RESULTS_DIR)/scheduling_experiments.csv
TABLES_TEX := $(RESULTS_DIR)/tables.tex

# src/*.c inclui o src/main.c real. experiment_runner.c vive em
# scripts/ mas e compilado junto por depender de include/scheduler.h.
SOURCES := $(wildcard $(SRC_DIR)/*.c) $(SCRIPTS_DIR)/experiment_runner.c
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(filter $(SRC_DIR)/%.c,$(SOURCES))) \
           $(BUILD_DIR)/experiment_runner.o

TARGET := $(BUILD_DIR)/simulator

.PHONY: all run deps plots tables reports clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/experiment_runner.o: $(SCRIPTS_DIR)/experiment_runner.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Roda o simulador em C e gera results/scheduling_experiments.csv +
# results/failed_runs.log (o proprio binario cria results/ se faltar).
run: $(TARGET)
	./$(TARGET)

# Instala as dependencias Python dos scripts de plots/tabelas
# (matplotlib, numpy, pandas), a partir de requirements.txt.
# Em sistemas com Python "externally managed" (PEP 668 -- Debian/Ubuntu
# recentes, muitos Codespaces), rode:
#   make deps PIP_FLAGS=--break-system-packages
# ou, preferivelmente, use um virtualenv:
#   python3 -m venv .venv && source .venv/bin/activate && make deps
deps:
	$(PYTHON) -m pip install $(PIP_FLAGS) -r requirements.txt

# Graficos (PNG) a partir do CSV de resultados. Depende de run para
# garantir que o CSV existe e esta atualizado.
plots: run
	$(PYTHON) $(SCRIPTS_DIR)/generate_plots.py $(CSV_PATH) --output-dir $(PLOTS_DIR)

# Tabelas LaTeX a partir do mesmo CSV.
tables: run
	$(PYTHON) $(SCRIPTS_DIR)/generate_latex_tables.py $(CSV_PATH) --output $(TABLES_TEX)

# Fluxo completo: compila, roda a simulacao e gera graficos + tabelas.
reports: plots tables

clean:
	rm -rf $(BUILD_DIR)

#make deps PIP_FLAGS=--break-system-packages