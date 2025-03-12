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
EVALUATION_REPEATS := 2

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

ANAL_ROOT_DIR := ./analysis
ANAL_OUTPUT_DIR := $(ANAL_ROOT_DIR)/output
ANAL_LOG_DIR := $(ANAL_ROOT_DIR)/logs
ANAL_SCRIPTS_DIR := $(ANAL_ROOT_DIR)/scripts
ANAL_REL_LAT_FILE := $(ANAL_ROOT_DIR)/system/rel_lat.txt

PYTHON_EXE := $(ANAL_ROOT_DIR)/scripts/.env/bin/python3

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
		[ -d $$dir ] && rm -rf $$dir/* && find $$dir -type d -empty -exec rmdir {} + || true; \
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

.PHONY: $(EVALUATION_CASES)
$(EVALUATION_CASES): %: $(EXECUTABLE)
	@echo "Running evaluation case: $@"
	@for template in $(shell ls $(EVAL_TEMPLATE_DIR)/$@/*.json 2>/dev/null); do \
		for i in $$(seq 1 $(EVALUATION_REPEATS)); do \
			TEMPLATE_NAME=$$(basename $$template .json); \
			ITERATION_SUFFIX=$${TEMPLATE_NAME}_iter_$${i}; \
			CONFIG_DIR=$(EVAL_CONFIG_DIR)/$@/$${TEMPLATE_NAME}; \
			CONFIG_FILE=$${CONFIG_DIR}/$${ITERATION_SUFFIX}; \
			OUTPUT_DIR=$(EVAL_OUTPUT_DIR)/$@/$${TEMPLATE_NAME}; \
			OUTPUT_FILE=$${OUTPUT_DIR}/$${ITERATION_SUFFIX}; \
			LOG_DIR=$(EVAL_LOG_DIR)/$@/$${TEMPLATE_NAME}; \
			LOG_FILE=$${LOG_DIR}/$${ITERATION_SUFFIX}.log; \
			mkdir -p "$$CONFIG_DIR" "$$OUTPUT_DIR" "$$LOG_DIR"; \
			$(PYTHON_EXE) $(EVAL_GENERATOR_DIR)/generate_config.py \
				--params log_base_name="$${OUTPUT_FILE}" \
				--template "$$template" \
				--output_file "$${CONFIG_FILE}.json" > "$${LOG_FILE}" 2>&1; \
			GEN_STATUS=$$?; \
			./$(EXECUTABLE) $(RUNTIME_LOG_FLAGS) "$$CONFIG_FILE.json" >> "$$LOG_FILE" 2>&1; \
			EXEC_STATUS=$$?; \
			$(PYTHON_EXE) $(TEST_VALIDATOR_DIR)/validate_offsets.py "$${OUTPUT_FILE}.yaml"  >> "$$LOG_FILE" 2>&1; \
			VAL_STATUS=$$?; \
			if [ $$GEN_STATUS -eq 0 ] && [ $$EXEC_STATUS -eq 0 ] && [ $$VAL_STATUS -eq 0 ]; then \
				echo "  [SUCCESS] $${ITERATION_SUFFIX}"; \
			else \
				echo "  [FAILED] $${ITERATION_SUFFIX} (Generate: $$GEN_STATUS, Execute: $$EXEC_STATUS, Validate: $$VAL_STATUS)"; \
			fi; \
			sleep 5; \
		done \
	done

# Run analysis
.PHONY: analysis
analysis:
	@echo "Running analysis..."
	@for out_dir in $$(ls $(EVAL_OUTPUT_DIR) 2>/dev/null); do \
		for sub_dir in $$(ls $(EVAL_OUTPUT_DIR)/$$out_dir 2>/dev/null); do \
			OUTPUT_DIR=$(ANAL_OUTPUT_DIR)/$$out_dir/$$sub_dir; \
			mkdir -p "$$OUTPUT_DIR"; \
			for yaml_file in $$(ls $(EVAL_OUTPUT_DIR)/$$out_dir/$$sub_dir/*.yaml); do \
				echo "  Generate profile and gantt $$yaml_file"; \
				BASE_NAME=$$(basename $$yaml_file .yaml); \
				PROFILE_DIR=$${OUTPUT_DIR}/profile; \
				PROFILE_NAME=$${PROFILE_DIR}/$${BASE_NAME}.csv; \
				GANTT_DIR=$${OUTPUT_DIR}/gantt; \
				GANTT_NAME=$${GANTT_DIR}/$${BASE_NAME}.png; \
				LOG_DIR=$(ANAL_LOG_DIR)/$$out_dir/$$sub_dir; \
				LOG_NAME=$${LOG_DIR}/$${BASE_NAME}.log; \
				mkdir -p "$$PROFILE_DIR" "$$GANTT_DIR" "$$LOG_DIR"; \
				$(PYTHON_EXE) $(ANAL_SCRIPTS_DIR)/generate_profile.py \
					"$$yaml_file" "$(ANAL_REL_LAT_FILE)" \
					--export_csv "$$PROFILE_NAME" \
					>> "$$LOG_NAME" 2>&1; \
				$(PYTHON_EXE) $(ANAL_SCRIPTS_DIR)/generate_gantt.py \
					"$$yaml_file" "$$GANTT_NAME" \
					>> "$$LOG_NAME" 2>&1; \
			done; \
		done; \
	done

	@for out_dir in $$(ls $(ANAL_OUTPUT_DIR) 2>/dev/null); do \
		for sub_dir in $$(ls $(ANAL_OUTPUT_DIR)/$$out_dir 2>/dev/null); do \
			echo "  Generate summary $$sub_dir"; \
			OUTPUT_DIR=$(ANAL_OUTPUT_DIR)/$$out_dir/$$sub_dir; \
			SUMMARY_DIR=$(ANAL_OUTPUT_DIR)/$$out_dir/$$sub_dir/summary; \
			SUMMARY_NAME=$${SUMMARY_DIR}/summary_$${sub_dir}.csv; \
			AGGREG_DIR=$(ANAL_OUTPUT_DIR)/$$out_dir; \
			AGGREG_NAME=$${AGGREG_DIR}/aggreg_$${sub_dir}.csv; \
			LOG_DIR=$(ANAL_LOG_DIR)/$$out_dir/$$sub_dir; \
			LOG_NAME=$${LOG_DIR}/summary.log; \
			mkdir -p "$$SUMMARY_DIR"; \
			$(PYTHON_EXE) $(ANAL_SCRIPTS_DIR)/generate_summary.py \
				"$$OUTPUT_DIR/profile" \
				"$$SUMMARY_NAME" \
				"$$AGGREG_NAME" \
				>> "$$LOG_NAME" 2>&1; \
		done; \
	done

	@for out_dir in $$(ls $(ANAL_OUTPUT_DIR) 2>/dev/null); do \
		echo "  Generate plot $$out_dir"; \
		AGGREG_DIR=$(ANAL_OUTPUT_DIR)/$$out_dir; \
		PLOT_NAME=$${AGGREG_DIR}/aggreg_$${out_dir}.png; \
		LOG_DIR=$(ANAL_LOG_DIR)/$$out_dir/$$sub_dir; \
		LOG_NAME=$${LOG_DIR}/summary.log; \
		$(PYTHON_EXE) $(ANAL_SCRIPTS_DIR)/generate_summary_plot.py \
			"$$AGGREG_DIR" \
			"$$PLOT_NAME" \
			>> "$$LOG_NAME" 2>&1; \
	done
