# Compiler and Flags
CXX := g++
CXXFLAGS := -Wall -O2 -g #-DNLOG # Uncomment for debugging: -fsanitize=address
LIBS := -lsimgrid -lhwloc

# Header Files
HDR := common.hpp hardware.hpp scheduler_base.hpp scheduler_fifo.hpp \
       scheduler_eft.hpp scheduler_heft.hpp scheduler_min_min.hpp \
       mapper_base.hpp mapper_bare_metal.hpp mapper_simulation.hpp \
       runtime.hpp

# Source Files and Object Files
SRC := common.cpp hardware.cpp scheduler_base.cpp scheduler_fifo.cpp \
       scheduler_eft.cpp scheduler_heft.cpp scheduler_min_min.cpp \
       mapper_base.cpp mapper_bare_metal.cpp mapper_simulation.cpp \
       runtime.cpp main.cpp

OBJ := $(SRC:.cpp=.o)

# Output Binary
TARGET := scheduler

XBT_RUNTIME_LOG := \
	--log=fifo_scheduler.thres:debug

TESTS := min_min_simulation heft_simulation fifo_simulation fifo_bare_metal

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
%.o: %.cpp $(HDR)
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

.PHONY: $(TESTS)
$(TESTS): %: $(TARGET)
	@echo "Generating output and log folders for $@..."
	@mkdir -p "$(OUTPUT_DIR)/$@" "$(LOG_DIR)/$@"

	@echo "Running $@..."
	@for json in $$(ls $(CONFIG_DIR)/$@/*.json 2>/dev/null); do \
		./$(TARGET) $(XBT_RUNTIME_LOG) $$json > "$(LOG_DIR)/$@/$$(basename $$json .json).log" 2>&1; \
		python3 $(VALIDATOR_DIR)/validate_offsets.py "$(OUTPUT_DIR)/$@/$$(basename $$json .json).yaml" >> "$(LOG_DIR)/$@/$$(basename $$json .json).log" 2>&1; \
		python3 $(VALIDATOR_DIR)/validate_output_and_order_v2.py "$(OUTPUT_DIR)/$@/$$(basename $$json .json).yaml" "$(OUTPUT_EXPECTED_DIR)/$@/$$(basename $$json .json).yaml" --check-order >> "$(LOG_DIR)/$@/$$(basename $$json .json).log" 2>&1; \
	done

.PHONY: test
test: $(TESTS)
