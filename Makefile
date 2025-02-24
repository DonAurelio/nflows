# Compiler and Flags
CXX := g++
CXXFLAGS := -Wall -O2 -g -DNLOG # Uncomment for debugging: -fsanitize=address
LIBS := -lsimgrid -lhwloc

# Source Files and Object Files
SRC := common.cpp hardware.cpp scheduler_base.cpp scheduler_fifo.cpp \
       scheduler_eft.cpp scheduler_heft.cpp scheduler_min_min.cpp \
       mapper_base.cpp mapper_bare_metal.cpp mapper_simulation.cpp \
       runtime.cpp main.cpp

OBJ := $(SRC:.cpp=.o)

# Output Binary
TARGET := scheduler

# Define the runtime logging flags
XBT_RUNTIME_LOG = \
	--log=mapper_simulation.thresh:debug \
	--log=scheduler_eft.thresh:debug \
	--log=common.thresh:debug

# List of Directed Acyclic Graph (DAG) files for workflow scheduling
DAG_FILES := \
	./tests/workflows/data_redis_2.dot

# Configuration Files
TEST_DIR := ./tests
CONFIG_DIR := $(TEST_DIR)/config
CONFIG_GEN_TEMPLATE := $(TEST_DIR)/templates/template.json
CONFIG_GEN_SCRIPT := python3 ./python/generate_config.py \
    --template $(CONFIG_GEN_TEMPLATE) \
    --output_dir $(CONFIG_DIR) \
    --params \
        scheduler_type=MIN_MIN,HEFT \
        mapper_type=SIMULATION \
		"dag_file=$(DAG_FILES)"

# Validator Script
YAML_OUTPUT := $(TEST_DIR)/output.yaml
VALIDATOR_SCRIPT := python3 ./python/validate_offsets.py $(YAML_OUTPUT)

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
	rm -f $(OBJ) $(TARGET) $(TEST_DIR)/*.yaml

# Generate Configs and Run Tests
test: $(TARGET)
	@echo "Generating configuration files..."
	@$(CONFIG_GEN_SCRIPT)
	@CONFIG_FILES=$$(ls $(CONFIG_DIR)/*.json); \
	for json in $$CONFIG_FILES; do \
		echo "Running test for $$json"; \
		./$(TARGET) $$json; \
		$(VALIDATOR_SCRIPT); \
	done
	@echo "Cleaning up generated configuration files..."
	@rm -f $(CONFIG_DIR)/*.json $(TEST_DIR)/*.yaml
