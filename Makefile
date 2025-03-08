# Compiler and Flags
CXX := g++
CXXFLAGS := -Wall -O2 #-g #-DNLOG # Uncomment for debugging: -fsanitize=address
LIBS := -lsimgrid -lhwloc

# Header Files
HEADERS := common.hpp hardware.hpp scheduler_base.hpp scheduler_fifo.hpp \
           scheduler_eft.hpp scheduler_heft.hpp scheduler_min_min.hpp \
           mapper_base.hpp mapper_bare_metal.hpp mapper_simulation.hpp \
           runtime.hpp

# Source Files and Object Files
SOURCES := common.cpp hardware.cpp scheduler_base.cpp scheduler_fifo.cpp \
           scheduler_eft.cpp scheduler_heft.cpp scheduler_min_min.cpp \
           mapper_base.cpp mapper_bare_metal.cpp mapper_simulation.cpp \
           runtime.cpp main.cpp

OBJECTS := $(SOURCES:.cpp=.o)

# Output Binary
EXECUTABLE := scheduler

RUNTIME_LOG_FLAGS := \
	--log=fifo_scheduler.thres:debug \
	--log=heft_scheduler.thres:debug \
	--log=eft_scheduler.thres:debug

# Test and Evaluation Targets
TEST_CASES := \
	min_min_simulation \
	heft_simulation \
	fifo_simulation \
	fifo_bare_metal \
	heft_bare_metal \
	min_min_bare_metal

EVALUATION_CASES := min_min
EVALUATION_REPEATS := 1

# Directories
TEST_ROOT_DIR := ./tests
TEST_CONFIG_DIR := $(TEST_ROOT_DIR)/config
TEST_OUTPUT_DIR := $(TEST_ROOT_DIR)/output
TEST_EXPECTED_OUTPUT_DIR := $(TEST_ROOT_DIR)/output_expected
TEST_LOG_DIR := $(TEST_ROOT_DIR)/logs
TEST_VALIDATOR_DIR := $(TEST_ROOT_DIR)/validators

EVAL_ROOT_DIR := ./eval
EVAL_CONFIG_DIR := $(EVAL_ROOT_DIR)/config
EVAL_GENERATOR_DIR := $(EVAL_ROOT_DIR)/generators
EVAL_TEMPLATE_DIR := $(EVAL_ROOT_DIR)/templates
EVAL_OUTPUT_DIR := $(EVAL_ROOT_DIR)/output
EVAL_LOG_DIR := $(EVAL_ROOT_DIR)/logs

# Paths to clean
CLEAN_PATHS := $(OBJECTS) $(EXECUTABLE) \
               $(TEST_OUTPUT_DIR)/**/*.yaml $(TEST_LOG_DIR)/**/*.log \
               $(EVAL_OUTPUT_DIR)/**/*.yaml $(EVAL_LOG_DIR)/**/*.log \
               $(EVAL_CONFIG_DIR)/**/*.json

# Default target
all: $(EXECUTABLE)

# Build Target
$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

# Compile Object Files
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run the scheduler with a sample configuration
run: $(EXECUTABLE)
	./$(EXECUTABLE) $(CONFIG_GEN_TEMPLATE) $(RUNTIME_LOG_FLAGS)

# Clean Build Files
clean:
	@echo "Cleaning build and output files..."
	@rm -f $(CLEAN_PATHS)
	@for dir in $(TEST_OUTPUT_DIR) $(TEST_LOG_DIR) $(EVAL_OUTPUT_DIR) $(EVAL_LOG_DIR) $(EVAL_CONFIG_DIR); do \
		[ -d $$dir ] && find $$dir -type d -empty -exec rmdir {} + || true; \
	done

# Run test cases
.PHONY: $(TEST_CASES)
$(TEST_CASES): %: $(EXECUTABLE)
	@echo "Generating output and log folders for $@..."
	@mkdir -p "$(TEST_OUTPUT_DIR)/$@" "$(TEST_LOG_DIR)/$@"

	@echo "Running test case: $@"
	@for config_file in $$(ls $(TEST_CONFIG_DIR)/$@/*.json 2>/dev/null); do \
		TEST_NAME=$$(basename $$config_file .json); \
		./$(EXECUTABLE) $(RUNTIME_LOG_FLAGS) $$config_file > "$(TEST_LOG_DIR)/$@/$$TEST_NAME.log" 2>&1; \
		python3 $(TEST_VALIDATOR_DIR)/validate_offsets.py "$(TEST_OUTPUT_DIR)/$@/$$TEST_NAME.yaml" >> "$(TEST_LOG_DIR)/$@/$$TEST_NAME.log" 2>&1; \
		python3 $(TEST_VALIDATOR_DIR)/validate_output.py --check-order exec_name_total_offsets \
			"$(TEST_OUTPUT_DIR)/$@/$$TEST_NAME.yaml" "$(TEST_EXPECTED_OUTPUT_DIR)/$@/$$TEST_NAME.yaml" \
			>> "$(TEST_LOG_DIR)/$@/$$TEST_NAME.log" 2>&1; \
	done

.PHONY: test
test: $(TEST_CASES)

# Run evaluation cases
.PHONY: $(EVALUATION_CASES)
$(EVALUATION_CASES): %: $(EXECUTABLE)
	@echo "Generating output and log folders for evaluation: $@"
	@mkdir -p "$(EVAL_OUTPUT_DIR)/$@" "$(EVAL_LOG_DIR)/$@" "$(EVAL_CONFIG_DIR)/$@"

	@echo "Running evaluation case: $@"
	@for template in $$(ls $(EVAL_TEMPLATE_DIR)/$@/*.json 2>/dev/null); do \
		for i in $$(seq 1 $(EVALUATION_REPEATS)); do \
			TEMPLATE_NAME=$$(basename $$template .json); \
			ITERATION_SUFFIX="$${TEMPLATE_NAME}_iter_$${i}"; \
			python3 $(EVAL_GENERATOR_DIR)/generate_config.py \
				--params log_base_name="$(EVAL_OUTPUT_DIR)/$@/$$ITERATION_SUFFIX" \
				--template $$template \
				--output_file "$(EVAL_CONFIG_DIR)/$@/$${ITERATION_SUFFIX}.json" > "$(EVAL_LOG_DIR)/$@/$${ITERATION_SUFFIX}.log" 2>&1; \
			./$(EXECUTABLE) $(RUNTIME_LOG_FLAGS) "$(EVAL_CONFIG_DIR)/$@/$${ITERATION_SUFFIX}.json" >> "$(EVAL_LOG_DIR)/$@/$${ITERATION_SUFFIX}.log" 2>&1; \
			python3 $(TEST_VALIDATOR_DIR)/validate_offsets.py "$(EVAL_OUTPUT_DIR)/$@/$${ITERATION_SUFFIX}.yaml" >> "$(EVAL_LOG_DIR)/$@/$${ITERATION_SUFFIX}.log" 2>&1; \
		done \
	done

.PHONY: eval
eval: $(EVALUATION_CASES)
