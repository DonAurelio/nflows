# Compiler and Flags
CXX := g++
CXXFLAGS := -Wall -O2 #-g #-DNLOG # Uncomment for debugging: -fsanitize=address
LIBS := -lsimgrid -lhwloc

# Files
HEADERS := $(wildcard *.hpp)
SOURCES := $(wildcard *.cpp)
OBJECTS := $(SOURCES:.cpp=.o)
EXECUTABLE := scheduler

RUNTIME_LOG_FLAGS := \
	--log=fifo_scheduler.thres:debug \
	--log=heft_scheduler.thres:debug \
	--log=eft_scheduler.thres:debug \
	--log=hardware.thres:debug

SCRIPTS_DIR := ./scripts
PYTHON_EXEC := $(SCRIPTS_DIR)/.env/bin/python3

SCRIPTS_VALIDATE_OFFSETS := $(PYTHON_EXEC) $(SCRIPTS_DIR)/validate_offsets.py
SCRIPTS_VALIDATE_OUTPUT := $(PYTHON_EXEC) $(SCRIPTS_DIR)/validate_output.py
SCRIPTS_GENERATE_CONFIG := $(PYTHON_EXEC) $(SCRIPTS_DIR)/generate_config.py
SCRIPTS_GENERATE_GANTT := $(PYTHON_EXEC) $(SCRIPTS_DIR)/generate_gantt.py
SCRIPTS_GENERATE_PROFILE := $(PYTHON_EXEC) $(SCRIPTS_DIR)/generate_profile.py
SCRIPTS_GENERATE_SUMMARY := $(PYTHON_EXEC) $(SCRIPTS_DIR)/generate_summary_aggreg.py
SCRIPTS_GENERATE_SUMMARY_PLOT := $(PYTHON_EXEC) $(SCRIPTS_DIR)/generate_summary_aggred_plot_v3.0.PY

# Directories
TEST_DIR := ./tests
TEST_CONFIG_DIR := $(TEST_DIR)/config
TEST_OUTPUT_DIR := $(TEST_DIR)/output
TEST_EXPECTED_DIR := $(TEST_DIR)/expected_output
TEST_LOG_DIR := $(TEST_DIR)/logs

# Test Targets
TEST_CASES := $(patsubst $(TEST_CONFIG_DIR)/%,%,$(wildcard $(TEST_CONFIG_DIR)/test_*))

EVAL_DIR := ./evaluation
EVAL_CONFIG_DIR := $(EVAL_DIR)/config
EVAL_GENERATOR_DIR := $(EVAL_DIR)/generators
EVAL_TEMPLATE_DIR := $(EVAL_DIR)/templates
EVAL_OUTPUT_DIR := $(EVAL_DIR)/output
EVAL_LOG_DIR := $(EVAL_DIR)/logs

EVALUATION_CASES := $(patsubst $(EVAL_TEMPLATE_DIR)/%,%,$(wildcard $(EVAL_TEMPLATE_DIR)/*))
EVALUATION_REPEATS := 3
EVALUATION_WORKFLOWS := \
	./eval/workflows/montage-chameleon-2mass-005d-001.dot \
	./eval/workflows/montage-chameleon-2mass-01d-001.dot \
	./eval/workflows/montage-chameleon-2mass-015d-001.dot \
	./eval/workflows/montage-chameleon-2mass-025d-001.dot

# EVALUATION_WORKFLOWS := \
# 	./eval/workflows/redis_4.dot \
# 	./eval/workflows/redis_8.dot \
# 	./eval/workflows/redis_16.dot \
# 	./eval/workflows/redis_32.dot

# EVALUATION_WORKFLOWS := \
# 	./eval/workflows/dis_4.dot \
# 	./eval/workflows/dis_8.dot \
# 	./eval/workflows/dis_16.dot \
# 	./eval/workflows/dis_32.dot

ANALYSIS_DIR := ./analysis
ANALYSIS_OUTPUT_DIR := $(ANALYSIS_DIR)/output
ANALYSIS_LOG_DIR := $(ANALYSIS_DIR)/logs
ANALYSIS_REL_LATENCY_FILE := $(ANALYSIS_DIR)/system/rel_lat.txt

BACKUP_DIR := ./backups
TIMESTAMP := $(shell date +"%Y%m%d_%H%M%S")
BACKUP_FILE := $(BACKUP_DIR)/$(TIMESTAMP).zip
BACKUP_DIRS := \
	$(ANALYSIS_OUTPUT_DIR) \
	$(ANALYSIS_LOG_DIR) \
	$(EVAL_CONFIG_DIR) \
	$(EVAL_LOG_DIR) \
	$(EVAL_OUTPUT_DIR)

EXAMPLE_DIR := ./example
EXAMPLE_CONFIG_FILE := $(EXAMPLE_DIR)/config_1.json
EXAMPLE_OUTPUT_FILE := $(EXAMPLE_DIR)/config_1.yaml
EXAMPLE_GANTT_FILE := $(EXAMPLE_DIR)/gantt.png
EXAMPLE_REL_LATENCY_FILE := $(ANALYSIS_DIR)/system/rel_lat.txt

# Paths to clean
CLEAN_FILES := \
	$(OBJECTS) \
	$(EXECUTABLE) \
    $(TEST_OUTPUT_DIR)/**/*.yaml \
	$(TEST_LOG_DIR)/**/*.log \
	$(EVAL_OUTPUT_DIR)/**/*.yaml \
	$(EVAL_LOG_DIR)/**/*.log \
    $(EVAL_CONFIG_DIR)/**/*.json \
	$(EXAMPLE_OUTPUT_FILE) \
	$(EXAMPLE_GANTT_FILE)

CLEAN_PATHS := \
	$(TEST_OUTPUT_DIR) \
	$(TEST_LOG_DIR) \
	$(EVAL_OUTPUT_DIR) \
	$(EVAL_LOG_DIR) \
	$(EVAL_CONFIG_DIR) \
	$(ANALYSIS_OUTPUT_DIR) \
	$(ANALYSIS_LOG_DIR)

# Default target
all: $(EXECUTABLE)

# Build Target
$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

# Compile Object Files
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

print-%:
	@echo '$*=$($*)'

# Run the scheduler with a sample configuration
run: $(EXECUTABLE)
	@./$(EXECUTABLE) $(EXAMPLE_CONFIG_FILE) $(RUNTIME_LOG_FLAGS)
	@$(SCRIPTS_VALIDATE_OFFSETS) $(EXAMPLE_OUTPUT_FILE)
	@$(SCRIPTS_GENERATE_GANTT) $(EXAMPLE_OUTPUT_FILE) $(EXAMPLE_GANTT_FILE)
	@$(SCRIPTS_GENERATE_PROFILE) $(EXAMPLE_OUTPUT_FILE) $(EXAMPLE_REL_LATENCY_FILE)

# Creates a zip archive before cleaning
# Backup task with status handling
.PHONY: backup
backup:
	@echo "Creating backup before cleaning..."
	@mkdir -p $(BACKUP_DIR)
	@LOG_FILE=$(BACKUP_FILE).log; \
	zip -r $(BACKUP_FILE) $(BACKUP_DIRS) > "$$LOG_FILE" 2>&1; \
	BACKUP_STATUS=$$?; \
	if [ $$BACKUP_STATUS -eq 0 ]; then \
		echo "  [SUCCESS] Backup saved to $(BACKUP_FILE)"; \
	else \
		echo "  [FAILED] Backup failed (Status: $$BACKUP_STATUS)"; \
	fi

# Clean Build Files
clean: backup
	@echo "Cleaning build and output files..."
	@rm -f $(CLEAN_FILES)
	@for dir in $(CLEAN_PATHS); do \
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
		LOG_FILE="$(TEST_LOG_DIR)/$@/$$TEST_NAME.log"; \
		OUTPUT_FILE="$(TEST_OUTPUT_DIR)/$@/$$TEST_NAME.yaml"; \
		EXPECTED_FILE="$(TEST_EXPECTED_DIR)/$@/$$TEST_NAME.yaml"; \
		./$(EXECUTABLE) $(RUNTIME_LOG_FLAGS) $$config_file > "$$LOG_FILE" 2>&1; \
		EXEC_STATUS=$$?; \
		$(SCRIPTS_VALIDATE_OFFSETS) "$$OUTPUT_FILE" >> "$$LOG_FILE" 2>&1; \
		VAL_OFFSETS_STATUS=$$?; \
		$(SCRIPTS_VALIDATE_OUTPUT) --check-order exec_name_total_offsets \
			"$$OUTPUT_FILE" "$$EXPECTED_FILE" >> "$$LOG_FILE" 2>&1; \
		VAL_OUTPUT_STATUS=$$?; \
		if [ $$EXEC_STATUS -eq 0 ] && [ $$VAL_OFFSETS_STATUS -eq 0 ] && [ $$VAL_OUTPUT_STATUS -eq 0 ]; then \
			echo "  [SUCCESS] $$config_file"; \
		else \
			echo "  [FAILED] $$config_file (Execute: $$EXEC_STATUS, Validate_offsets: $$VAL_OFFSETS_STATUS, Validate_ouptut: $$VAL_OUTPUT_STATUS)"; \
		fi; \
	done

.PHONY: test
test: $(TEST_CASES)

.PHONY: $(EVALUATION_CASES)
$(EVALUATION_CASES): %: $(EXECUTABLE)
	@echo "Running evaluation case: $@"
	@for template in $(shell ls $(EVAL_TEMPLATE_DIR)/$@/*.json 2>/dev/null); do \
		for workflow_path in $(EVALUATION_WORKFLOWS); do \
			WORKFLOW_NAME=$$(basename $$workflow_path .dot); \
			for i in $$(seq 1 $(EVALUATION_REPEATS)); do \
				TEMPLATE_NAME=$$(basename $$template .json); \
				ITERATION_SUFFIX=$${TEMPLATE_NAME}_$${WORKFLOW_NAME}_iter_$${i}; \
				CONFIG_DIR=$(EVAL_CONFIG_DIR)/$@/$${TEMPLATE_NAME}/$${WORKFLOW_NAME}; \
				CONFIG_FILE=$${CONFIG_DIR}/$${ITERATION_SUFFIX}; \
				OUTPUT_DIR=$(EVAL_OUTPUT_DIR)/$@/$${TEMPLATE_NAME}/$${WORKFLOW_NAME}; \
				OUTPUT_FILE=$${OUTPUT_DIR}/$${ITERATION_SUFFIX}; \
				LOG_DIR=$(EVAL_LOG_DIR)/$@/$${TEMPLATE_NAME}; \
				LOG_FILE=$${LOG_DIR}/$${ITERATION_SUFFIX}.log; \
				mkdir -p "$$CONFIG_DIR" "$$OUTPUT_DIR" "$$LOG_DIR"; \
				$(SCRIPTS_GENERATE_CONFIG) \
					--params log_base_name="$${OUTPUT_FILE}" dag_file="$${workflow_path}" \
					--template "$$template" \
					--output_file "$${CONFIG_FILE}.json" > "$${LOG_FILE}" 2>&1; \
				GEN_STATUS=$$?; \
				./$(EXECUTABLE) $(RUNTIME_LOG_FLAGS) "$$CONFIG_FILE.json" >> "$$LOG_FILE" 2>&1; \
				EXEC_STATUS=$$?; \
				$(SCRIPTS_VALIDATE_OFFSETS) "$${OUTPUT_FILE}.yaml"  >> "$$LOG_FILE" 2>&1; \
				VAL_STATUS=$$?; \
				if [ $$GEN_STATUS -eq 0 ] && [ $$EXEC_STATUS -eq 0 ] && [ $$VAL_STATUS -eq 0 ]; then \
					echo "  [SUCCESS] $${ITERATION_SUFFIX}"; \
				else \
					echo "  [FAILED] $${ITERATION_SUFFIX} (Generate: $$GEN_STATUS, Execute: $$EXEC_STATUS, Validate: $$VAL_STATUS)"; \
				fi; \
				sleep 5; \
			done \
		done \
	done

.PHONY: evaluate
evaluate: $(EVALUATION_CASES)

.PHONY: analyze
analyze:
	@echo "Running analysis..."
	@for out_dir in $$(ls $(EVAL_OUTPUT_DIR) 2>/dev/null); do \
		for sub_dir in $$(ls $(EVAL_OUTPUT_DIR)/$$out_dir 2>/dev/null); do \
			for workflow_dir in $$(ls $(EVAL_OUTPUT_DIR)/$$out_dir/$$sub_dir 2>/dev/null); do \
				OUTPUT_DIR=$(ANALYSIS_OUTPUT_DIR)/$$out_dir/$$sub_dir/$$workflow_dir; \
				mkdir -p "$$OUTPUT_DIR"; \
				for yaml_file in $$(ls $(EVAL_OUTPUT_DIR)/$$out_dir/$$sub_dir/$$workflow_dir/*.yaml 2>/dev/null); do \
					BASE_NAME=$$(basename $$yaml_file .yaml); \
					PROFILE_DIR=$${OUTPUT_DIR}/profile; \
					PROFILE_NAME=$${PROFILE_DIR}/$${BASE_NAME}.csv; \
					GANTT_DIR=$${OUTPUT_DIR}/gantt; \
					GANTT_NAME=$${GANTT_DIR}/$${BASE_NAME}.png; \
					LOG_DIR=$(ANALYSIS_LOG_DIR)/$$out_dir/$$sub_dir; \
					LOG_NAME=$${LOG_DIR}/$${BASE_NAME}.log; \
					mkdir -p "$$PROFILE_DIR" "$$GANTT_DIR" "$$LOG_DIR"; \
					$(SCRIPTS_GENERATE_PROFILE) \
						"$$yaml_file" "$(ANALYSIS_REL_LATENCY_FILE)" \
						--export_csv "$$PROFILE_NAME" \
						>> "$$LOG_NAME" 2>&1; \
					PROFILE_STATUS=$$?; \
					$(SCRIPTS_GENERATE_GANTT) \
						"$$yaml_file" "$$GANTT_NAME" \
						>> "$$LOG_NAME" 2>&1; \
					GANTT_STATUS=$$?; \
					if [ $$PROFILE_STATUS -eq 0 ] && [ $$GANTT_STATUS -eq 0 ]; then \
						echo "  [SUCCESS] Profile & Gantt: $$yaml_file"; \
					else \
						echo "  [FAILED] Profile & Gantt: $$yaml_file (Profile: $$PROFILE_STATUS, Gantt: $$GANTT_STATUS)"; \
					fi; \
				done; \
			done; \
		done; \
	done

	@for out_dir in $$(ls $(ANALYSIS_OUTPUT_DIR) 2>/dev/null); do \
		for sub_dir in $$(ls $(ANALYSIS_OUTPUT_DIR)/$$out_dir 2>/dev/null); do \
			for workflow_dir in $$(ls $(ANALYSIS_OUTPUT_DIR)/$$out_dir/$$sub_dir 2>/dev/null); do \
				OUTPUT_DIR=$(ANALYSIS_OUTPUT_DIR)/$$out_dir/$$sub_dir/$$workflow_dir; \
				SUMMARY_DIR=$${OUTPUT_DIR}/summary; \
				SUMMARY_NAME=$${SUMMARY_DIR}/summary_$${sub_dir}.csv; \
				AGGREG_DIR=$(ANALYSIS_OUTPUT_DIR)/$$out_dir; \
				AGGREG_NAME=$${AGGREG_DIR}/aggreg_$${sub_dir}_$${workflow_dir}.csv; \
				LOG_DIR=$(ANALYSIS_LOG_DIR)/$$out_dir/$$sub_dir; \
				LOG_NAME=$${LOG_DIR}/summary.log; \
				mkdir -p "$$SUMMARY_DIR"; \
				$(SCRIPTS_GENERATE_SUMMARY) \
					"$$OUTPUT_DIR/profile" \
					"$$SUMMARY_NAME" \
					"$$AGGREG_NAME" \
					>> "$$LOG_NAME" 2>&1; \
				SUMMARY_STATUS=$$?; \
				if [ $$SUMMARY_STATUS -eq 0 ]; then \
					echo "  [SUCCESS] Summary: $$sub_dir"; \
				else \
					echo "  [FAILED] Summary: $$sub_dir (Status: $$SUMMARY_STATUS)"; \
				fi; \
			done; \
		done; \
	done

	@for out_dir in $$(ls $(ANALYSIS_OUTPUT_DIR) 2>/dev/null); do \
		AGGREG_DIR=$(ANALYSIS_OUTPUT_DIR)/$$out_dir; \
		PLOT_NAME=$${AGGREG_DIR}/aggreg_$${out_dir}.png; \
		LOG_DIR=$(ANALYSIS_LOG_DIR)/$$out_dir; \
		LOG_NAME=$${LOG_DIR}/summary.log; \
		$(SCRIPTS_GENERATE_SUMMARY_PLOT) \
			"$$AGGREG_DIR" \
			"$$PLOT_NAME" \
			>> "$$LOG_NAME" 2>&1; \
		PLOT_STATUS=$$?; \
		if [ $$PLOT_STATUS -eq 0 ]; then \
			echo "  [SUCCESS] Plot: $$out_dir"; \
		else \
			echo "  [FAILED] Plot: $$out_dir (Status: $$PLOT_STATUS)"; \
		fi; \
	done
