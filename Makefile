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

EVALUATION_DIR := ./evaluation
EVALUATION_TEMPLATE_DIR := $(EVALUATION_DIR)/templates
EVALUATION_WORKFLOW_DIR := $(EVALUATION_DIR)/workflows

EVALUATION_RESULT_DIR := ./results
EVALUATION_LOG_DIR := $(EVALUATION_RESULT_DIR)/log
EVALUATION_OUTPUT_DIR := $(EVALUATION_RESULT_DIR)/output
EVALUATION_CONFIG_DIR := $(EVALUATION_RESULT_DIR)/config

EVALUATION_REPEATS := 2
EVALUATION_WORKFLOWS := dis_16.dot
EVALUATION_GROUPS := example
EVALUATION_SLEEPTIME := 10

ANALYSIS_WORKFLOWS := $(notdir $(shell find $(EVALUATION_OUTPUT_DIR) -mindepth 1 -maxdepth 1 -type d 2>/dev/null))
ANALYSIS_REL_LATENCIES_FILE := $(EVALUATION_DIR)/system/non_uniform_lat_rel.txt
ANALYSIS_FIELDS := \
	workflow_makespan_us \
	local_accesses \
	remote_accesses \
	access_pattern_performance \
	checksum \
	active_threads \
	total_accesses \
	total_write_time_us \
	total_compute_time_us \
	comp_to_comm_ratio \
	comm_to_comp_ratio \
	memory_migrations \
	read_footprint_bytes \
	write_footprint_bytes \
	compute_footprint_flops

# Directories
TEST_DIR := ./tests
TEST_CONFIG_DIR := $(TEST_DIR)/config
TEST_EXPECTED_DIR := $(TEST_DIR)/expected

TEST_LOG_DIR := $(TEST_DIR)/log
TEST_OUTPUT_DIR := $(TEST_DIR)/output

# Test Targets
TEST_CASES := $(patsubst $(TEST_CONFIG_DIR)/%,%,$(wildcard $(TEST_CONFIG_DIR)/test_*))

BACKUP_DIR := ./backups
BACKUP_DIRS := \
	$(EVALUATION_RESULT_DIR) \
	$(EVALUATION_DIR)

CLEAN_PATHS := \
	$(EVALUATION_RESULT_DIR) \
	$(TEST_OUTPUT_DIR) \
	$(TEST_LOG_DIR)

CLEAN_FILES := \
	$(OBJECTS) \
	$(EXECUTABLE)

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

.PHONY: backup
backup:
	@mkdir -p $(BACKUP_DIR)
	@BASE_NAME=$(shell date +"%Y%m%d_%H%M%S"); \
	BACKUP_FILE=$(BACKUP_DIR)/$${BASE_NAME}.zip; \
	LOG_FILE=$(BACKUP_DIR)/$${BASE_NAME}.log; \
	if zip -r $$BACKUP_FILE $(BACKUP_DIRS) > $$LOG_FILE 2>&1; then \
		echo "Backup saved to $$BACKUP_FILE"; \
	else \
		echo "Backup failed"; \
	fi

.PHONY: clean
clean: backup
	@rm -f $(CLEAN_FILES)
	@for dir in $(CLEAN_PATHS); do \
		echo "Cleaning $$dir";\
		[ -d $$dir ] && rm -rf $$dir/* && find $$dir -type d -empty -exec rmdir {} + || true; \
	done

# Run test cases
.PHONY: $(TEST_CASES)
$(TEST_CASES): %: $(EXECUTABLE)
	@echo "Running test case: $@"
	@mkdir -p "$(TEST_OUTPUT_DIR)/$@" "$(TEST_LOG_DIR)/$@"
	@for config_file in $(wildcard $(TEST_CONFIG_DIR)/$@/*.json); do \
		BASE_NAME=$$(basename $$config_file .json); \
		LOG_FILE="$(TEST_LOG_DIR)/$@/$${BASE_NAME}.log"; \
		OUTPUT_FILE="$(TEST_OUTPUT_DIR)/$@/$${BASE_NAME}.yaml"; \
		EXPECTED_FILE="$(TEST_EXPECTED_DIR)/$@/$${BASE_NAME}.yaml"; \
		./$(EXECUTABLE) $(RUNTIME_LOG_FLAGS) $$config_file > "$$LOG_FILE" 2>&1 && \
		$(VALIDATE_OFFSETS) "$$OUTPUT_FILE" >> "$$LOG_FILE" 2>&1 && \
		$(VALIDATE_OUTPUT) --check-order exec_name_total_offsets "$$OUTPUT_FILE" "$$EXPECTED_FILE" >> "$$LOG_FILE" 2>&1 && \
		echo "  [SUCCESS] $$config_file" || \
		echo "  [FAILED] $$config_file"; \
	done

.PHONY: test
test: $(TEST_CASES)

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
			--export_csv "$${PROFILE_DIR}/$$(basename $${output_yaml} .yaml).csv" >> "$${LOG_FILE}" 2>&1; \
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

	@for profiles_dir in $$(find $(EVALUATION_RESULT_DIR)/profile/$@/**/** -type d 2>/dev/null); do \
		AGGREG_DIR=$$(dirname $${profiles_dir} | sed 's/profile/aggreg/'); \
		AGGREG_FILE=$${AGGREG_DIR}/$$(basename $${profiles_dir}).csv; \
		SUMMARY_DIR=$$(dirname $${profiles_dir} | sed 's/profile/summary/'); \
		SUMMARY_FILE=$${SUMMARY_DIR}/$$(basename $${profiles_dir}).csv; \
		LOG_DIR=$$(dirname $${profiles_dir} | sed 's/profile/log/'); \
		LOG_FILE=$${LOG_DIR}/$$(basename $${profiles_dir}).log; \
		mkdir -p $${AGGREG_DIR} $${SUMMARY_DIR} $${LOG_DIR}; \
		$(GENERATE_AGGREG) \
			"$${profiles_dir}" \
			"$${SUMMARY_FILE}" \
			"$${AGGREG_FILE}" >> "$${LOG_FILE}" 2>&1; \
		AGGREG_STATUS=$$?; \
		if [ $$AGGREG_STATUS -eq 0 ]; then \
			echo "  [SUCCESS] Aggreg: $$profiles_dir"; \
		else \
			echo "  [FAILED] Aggreg: $$profiles_dir (Aggreg: $$AGGREG_STATUS)"; \
		fi; \
	done

	@for aggreg_dir in $$(find $(EVALUATION_RESULT_DIR)/aggreg/$@/** -maxdepth 0 -type d 2>/dev/null); do \
		FIGURE_DIR=$$(dirname $${aggreg_dir} | sed 's/aggreg/figure/'); \
		LOG_DIR=$$(dirname $${aggreg_dir} | sed 's/aggreg/log/'); \
		LOG_FILE=$${LOG_DIR}/$$(basename $${aggreg_dir}).log; \
		mkdir -p $${FIGURE_DIR} $${LOG_DIR}; \
		for field in $(ANALYSIS_FIELDS); do \
			$(GENERATE_AGGREG_PLOT) \
				"$${aggreg_dir}" \
				"$${FIGURE_DIR}/$$(basename $${aggreg_dir})_$${field}.png" \
				--field_name=$${field} >> "$${LOG_FILE}" 2>&1; \
			PLOT_STATUS=$$?; \
			if [ $$PLOT_STATUS -eq 0 ]; then \
				echo "  [SUCCESS] Plot: $$aggreg_dir ($${field})"; \
			else \
				echo "  [FAILED] Plot: $$aggreg_dir ($${field}) (Plot: $$PLOT_STATUS)"; \
			fi; \
		done; \
	done
