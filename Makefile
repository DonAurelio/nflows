# Compiler and Flags
CXX := g++
CXXFLAGS := -Wall -O2 -g #-DNLOG # Uncomment for debugging: -fsanitize=address
LIBS := -lsimgrid -lhwloc

# Source Files and Object Files
SRC := common.cpp hardware.cpp scheduler_base.cpp scheduler_fifo.cpp \
       scheduler_eft.cpp scheduler_heft.cpp scheduler_min_min.cpp \
       mapper_base.cpp mapper_bare_metal.cpp mapper_simulation.cpp \
       runtime.cpp main.cpp

OBJ := $(SRC:.cpp=.o)

# Output Binary
TARGET := scheduler

# Define available schedulers and execution platforms
SCHEDULERS := min_min
MAPPERS := simulation

# Define the runtime logging flags
XBT_RUNTIME_LOG = \
	--log=mapper_simulation.thresh:debug \
	--log=scheduler_eft.thresh:debug \
	--log=common.thresh:debug

# List of Directed Acyclic Graph (DAG) files for workflow scheduling
DAG_FILES := \
	./tests/workflows/data_redis_2.dot

# Directories
TEST_DIR := ./tests
CONFIG_DIR := $(TEST_DIR)/config
OUTPUT_DIR := $(TEST_DIR)/output
OUTPUT_EXPECTED_DIR = $(TEST_DIR)/output_expected
LOG_DIR := $(TEST_DIR)/logs
VALIDATOR_DIR := $(TEST_DIR)/validators

# Default target
all: $(TARGET)

# Build Target
$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

# Compile Object Files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run the scheduler with a sample configuration
run: $(TARGET)
	./$(TARGET) $(CONFIG_GEN_TEMPLATE) $(XBT_RUNTIME_LOG)

# Clean Build Files
clean:
	@rm -f $(OBJ) $(TARGET) $(OUTPUT_DIR)/**/*.yaml $(LOG_DIR)/**/*.log
	@[ -d $(OUTPUT_DIR) ] && find $(OUTPUT_DIR) -type d -empty -exec rmdir {} + || true
	@[ -d $(LOG_DIR) ] && find $(LOG_DIR) -type d -empty -exec rmdir {} + || true
	@[ -d $(OUTPUT_DIR) ] && rmdir $(OUTPUT_DIR) 2>/dev/null || true
	@[ -d $(LOG_DIR) ] && rmdir $(LOG_DIR) 2>/dev/null || true

test-min_min-simulation: $(TARGET)
	@echo "Generating output and log folders for min_min on simulation..."
	@mkdir -p "$(OUTPUT_DIR)/min_min_simulation" "$(LOG_DIR)/min_min_simulation"

	@echo "Running min_min on simulation..."
	@for json in $$(ls $(CONFIG_DIR)/min_min_simulation/*.json 2>/dev/null); do \
		./$(TARGET) $$json > $(LOG_DIR)/min_min_simulation/`basename $$json .json`.log 2>&1; \
		python3 $(VALIDATOR_DIR)/validate_offsets.py $(OUTPUT_DIR)/min_min_simulation/`basename $$json .json`.yaml >> $(LOG_DIR)/min_min_simulation/`basename $$json .json`.log 2>&1; \
		python3 $(VALIDATOR_DIR)/validate_output.py $(OUTPUT_DIR)/min_min_simulation/`basename $$json .json`.yaml $(OUTPUT_EXPECTED_DIR)/min_min_simulation/`basename $$json .json`.yaml >> $(LOG_DIR)/min_min_simulation/`basename $$json .json`.log 2>&1; \
	done
