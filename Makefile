# Compiler and Flags
CXX := g++
CXXFLAGS := -Wall -O2 #-g #-DNLOG # Uncomment for debugging: -fsanitize=address
LIBS := -lsimgrid -lhwloc

# Sources and Headers
HEADERS := $(wildcard *.hpp)
SOURCES := $(wildcard *.cpp)
OBJECTS := $(SOURCES:.cpp=.o)
EXECUTABLE := scheduler

# Runtime logging configuration
RUNTIME_LOG_FLAGS := \
	--log=fifo_scheduler.thres:debug \
	--log=heft_scheduler.thres:debug \
	--log=eft_scheduler.thres:debug \
	--log=hardware.thres:debug

# Directories and Executables
SCRIPTS_DIR := ./scripts
PYTHON_EXEC := $(SCRIPTS_DIR)/.env/bin/python3

# Validation Scripts
VALIDATE_OFFSETS := $(PYTHON_EXEC) $(SCRIPTS_DIR)/validate_offsets.py
VALIDATE_OUTPUT := $(PYTHON_EXEC) $(SCRIPTS_DIR)/validate_output.py

# Configuration Generation Scripts
GENERATE_CONFIG := $(PYTHON_EXEC) $(SCRIPTS_DIR)/generate_config.py

# Visualization and Reporting Scripts
GENERATE_GANTT := $(PYTHON_EXEC) $(SCRIPTS_DIR)/generate_gantt.py
GENERATE_PROFILE := $(PYTHON_EXEC) $(SCRIPTS_DIR)/generate_profile.py

# Data Aggregation and Plotting Scripts
GENERATE_AGGREG := $(PYTHON_EXEC) $(SCRIPTS_DIR)/generate_aggreg.py
GENERATE_AGGREG_PLOT := $(PYTHON_EXEC) $(SCRIPTS_DIR)/generate_aggreg_plot_v3.0.py

# Directories
TEST_DIR := ./tests
TEST_LOG_DIR := $(TEST_DIR)/logs
TEST_CONFIG_DIR := $(TEST_DIR)/config
TEST_OUTPUT_DIR := $(TEST_DIR)/output
TEST_EXPECTED_DIR := $(TEST_DIR)/expected

# Test Targets
TEST_CASES := $(patsubst $(TEST_CONFIG_DIR)/%,%,$(wildcard $(TEST_CONFIG_DIR)/test_*))

EVALUATION_DIR := ./evaluation
EVALUATION_TEMPLATE_DIR := $(EVALUATION_DIR)/templates
EVALUATION_WORKFLOW_DIR := $(EVALUATION_DIR)/workflows

EVALUATION_RESULT_DIR := ./results
EVALUATION_LOG_DIR := $(EVALUATION_RESULT_DIR)/log
EVALUATION_OUTPUT_DIR := $(EVALUATION_RESULT_DIR)/output
EVALUATION_CONFIG_DIR := $(EVALUATION_RESULT_DIR)/config

EVALUATION_REPEATS := 1
EVALUATION_WORKFLOWS := dis_16.dot
EVALUATION_GROUPS := example
EVALUATION_SLEEPTIME := 10

ANALYSIS_WORKFLOWS := $(notdir $(shell find $(EVALUATION_OUTPUT_DIR) -mindepth 1 -maxdepth 1 -type d))
ANALYSIS_REL_LATENCIES_FILE := $(EVALUATION_DIR)/system/non_uniform_lat_rel.txt

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

.PHONY: $(EVALUATION_WORKFLOWS)
$(EVALUATION_WORKFLOWS): %: $(EXECUTABLE)
	@echo "Running workflow: $@"
	@for group in $(EVALUATION_GROUPS); do \
		for json_template in $$(ls $(EVALUATION_TEMPLATE_DIR)/$$group/*.json 2>/dev/null); do \
			BASE_NAME=$$(basename $$json_template .json); \
			CONFIG_DIR=$(EVALUATION_CONFIG_DIR)/$$(basename $@ .dot)/$$group/$$BASE_NAME; \
			OUTPUT_DIR=$(EVALUATION_OUTPUT_DIR)/$$(basename $@ .dot)/$$group/$$BASE_NAME; \
			LOG_DIR=$(EVALUATION_LOG_DIR)/$$(basename $@ .dot)/$$group/$$BASE_NAME; \
			mkdir -p $$CONFIG_DIR $$OUTPUT_DIR $$LOG_DIR; \
			for repeat in $(shell seq 1 $(EVALUATION_REPEATS)); do \
				CONFIG_FILE=$$CONFIG_DIR/$${repeat}.json; \
				OUTPUT_FILE=$$OUTPUT_DIR/$${repeat}; \
				LOG_FILE=$$LOG_DIR/$${repeat}.log; \
				$(GENERATE_CONFIG) \
					--template "$$json_template" \
					--output_file "$$CONFIG_FILE" > "$$LOG_FILE" 2>&1 \
					--params log_base_name="$${OUTPUT_FILE}" dag_file="$(EVALUATION_WORKFLOW_DIR)/$@"; \
				GENERATE_STATUS=$$?; \
				./$(EXECUTABLE) $(RUNTIME_LOG_FLAGS) "$${CONFIG_FILE}" >> "$$LOG_FILE" 2>&1; \
				EXECUTABLE_STATUS=$$?; \
				$(VALIDATE_OFFSETS) "$${OUTPUT_FILE}.yaml"  >> "$$LOG_FILE" 2>&1; \
				VALIDATE_STATUS=$$?; \
				if [ $$GENERATE_STATUS -eq 0 ] && [ $$EXECUTABLE_STATUS -eq 0 ] && [ $$VALIDATE_STATUS -eq 0 ]; then \
					echo "  [SUCCESS] $$CONFIG_FILE"; \
				else \
					echo "  [FAILED] $$CONFIG_FILE (Generate: $$GENERATE_STATUS, Execute: $$EXECUTABLE_STATUS, Validate: $$VALIDATE_STATUS)"; \
				fi; \
				sleep $(EVALUATION_SLEEPTIME); \
			done; \
		done; \
	done

.PHONY: $(ANALYSIS_WORKFLOWS)
$(ANALYSIS_WORKFLOWS): %:
	@echo "Analyzing workflow: $@"
	@for output_yaml in $$(find $(EVALUATION_OUTPUT_DIR)/$@ -type f -name "*.yaml" 2>/dev/null); do \
		PROFILE_DIR=$$(dirname $${output_yaml} | sed 's/output/profile/'); \
		GANTT_DIR=$$(dirname $${output_yaml} | sed 's/output/gantt/'); \
		LOG_DIR=$$(dirname $${output_yaml} | sed 's/output/log/'); \
		LOG_FILE=$${LOG_DIR}/$$(basename $${output_yaml} .yaml).log; \
		mkdir -p $${PROFILE_DIR} $${GANTT_DIR} $${LOG_DIR}; \
		$(GENERATE_PROFILE) \
			"$${output_yaml}" \
			"$(ANALYSIS_REL_LATENCIES_FILE)" \
			--export_csv "$${PROFILE_DIR}/$$(basename $${output_yaml} .yaml).csv" > "$${LOG_FILE}" 2>&1; \
		PROFILE_STATUS=$$?; \
		$(GENERATE_GANTT) \
			"$${output_yaml}" \
			"$${GANTT_DIR}/$$(basename $${output_yaml} .yaml).png" >> "$${LOG_FILE}" 2>&1; \
		GANTT_STATUS=$$?; \
		if [ $$PROFILE_STATUS -eq 0 ] && [ $$GANTT_STATUS -eq 0 ]; then \
			echo "  [SUCCESS] Profile & Gantt: $$output_yaml"; \
		else \
			echo "  [FAILED] Profile & Gantt: $$output_yaml (Profile: $$PROFILE_STATUS, Gantt: $$GANTT_STATUS)"; \
		fi; \
	done
