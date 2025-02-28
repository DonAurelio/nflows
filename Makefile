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

SIMULATIONS := min_min_simulation heft_simulation

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

test-%: $(TARGET)
	@echo "Generating output and log folders for $*..."
	@mkdir -p "$(OUTPUT_DIR)/$*" "$(LOG_DIR)/$*"

	@echo "Running $*..."
	@for json in $$(ls $(CONFIG_DIR)/$*/*.json 2>/dev/null); do \
		./$(TARGET) $$json > "$(LOG_DIR)/$*/$$(basename $$json .json).log" 2>&1; \
		python3 $(VALIDATOR_DIR)/validate_offsets.py "$(OUTPUT_DIR)/$*/$$(basename $$json .json).yaml" >> "$(LOG_DIR)/$*/$$(basename $$json .json).log" 2>&1; \
		python3 $(VALIDATOR_DIR)/validate_output.py "$(OUTPUT_DIR)/$*/$$(basename $$json .json).yaml" "$(OUTPUT_EXPECTED_DIR)/$*/$$(basename $$json .json).yaml" >> "$(LOG_DIR)/$*/$$(basename $$json .json).log" 2>&1; \
	done
