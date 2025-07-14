# Compiler and Flags
CXX := g++
CXXFLAGS := -Wall -O2 -fno-prefetch-loop-arrays #-g #-DNLOG # Uncomment for debugging: -fsanitize=address
LIBS := -lsimgrid -lhwloc

# Sources and Headers
HEADERS := $(wildcard *.hpp)
SOURCES := $(wildcard *.cpp)
OBJECTS := $(SOURCES:.cpp=.o)
EXECUTABLE := nflows

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
GENERATE_SUBMIT := $(PYTHON_EXEC) $(SCRIPTS_DIR)/generate_submit.py

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
EVALUATION_SLURM_DIR := $(EVALUATION_RESULT_DIR)/slurm

EVALUATION_SLEEPTIME := 10
EVALUATION_REPEATS := 1
EVALUATION_GROUPS := $(notdir $(shell find $(EVALUATION_TEMPLATE_DIR) -mindepth 1 -maxdepth 1 -type d))
EVALUATION_WORKFLOWS := $(notdir $(shell find $(EVALUATION_WORKFLOW_DIR) -mindepth 1 -maxdepth 1 -type f -name "*.dot" 2>/dev/null))
EVALUATION_CONFIG_DIRS :=  $(notdir $(shell find $(EVALUATION_CONFIG_DIR) -mindepth 1 -maxdepth 1 -type d 2>/dev/null))

ANALYSIS_WORKFLOWS := $(notdir $(shell find $(EVALUATION_OUTPUT_DIR) -mindepth 1 -maxdepth 1 -type d 2>/dev/null))
ANALYSIS_REL_LATENCIES_FILE := $(EVALUATION_DIR)/system/non_uniform_lat_rel.txt
ANALYSIS_PROFILE_TIME_UNIT := s
ANALYSIS_PROFILE_PAYLOAD_UNIT := M
ANALYSIS_FIELDS := \
	data_pages_migrations \
	data_pages_spreadings \
	threads_checksum \
	threads_active \
	numa_factor \
	comp_to_comm \
	comm_to_comp \
	numa_awareness \
	makespan \
	read_time_local \
	read_time_remote \
	read_time_total \
	write_time_local \
	write_time_remote \
	write_time_total \
	compute_time_total \
	read_payload_local \
	read_payload_remote \
	read_payload_total \
	write_payload_local \
	write_payload_remote \
	write_payload_total \
	accesses_payload_total \
	compute_payload_total \
	read_accesses_local \
	read_accesses_remote \
	read_accesses_total \
	write_accesses_local \
	write_accesses_remote \
	write_accesses_total \
	accesses_total

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

# RUN TESTS
.PHONY: $(TEST_CASES)
$(TEST_CASES): %: $(EXECUTABLE)
	@echo "Running test case: $@"
	@mkdir -p "$(TEST_OUTPUT_DIR)/$@" "$(TEST_LOG_DIR)/$@"
	@for config_file in $(wildcard $(TEST_CONFIG_DIR)/$@/*.json); do \
		BASE_NAME=$$(basename $$config_file .json); \
		LOG_FILE="$(TEST_LOG_DIR)/$@/$${BASE_NAME}.log"; \
		OUTPUT_FILE="$(TEST_OUTPUT_DIR)/$@/$${BASE_NAME}.yaml"; \
		EXPECTED_FILE="$(TEST_EXPECTED_DIR)/$@/$${BASE_NAME}.yaml"; \
		START_TIME=$$(date +%s.%N); \
		./$(EXECUTABLE) $(RUNTIME_LOG_FLAGS) $$config_file > "$$LOG_FILE" 2>&1; \
		EXECUTABLE_STATUS=$$?; \
		END_TIME=$$(date +%s.%N); \
		ELAPSED_TIME_SEC=$$(echo "$$END_TIME - $$START_TIME" | bc); \
		printf "    Execution time: %.3f s\n" "$$ELAPSED_TIME_SEC" >> "$$LOG_FILE"; \
		$(VALIDATE_OFFSETS) "$$OUTPUT_FILE" >> "$$LOG_FILE" 2>&1; \
		VALIDATE_STATUS_OFFSETS=$$?; \
		$(VALIDATE_OUTPUT) --check-order exec_name_total_offsets "$$OUTPUT_FILE" "$$EXPECTED_FILE" >> "$$LOG_FILE" 2>&1; \
		VALIDATE_STATUS_OUTPUT=$$?; \
		if [ $$EXECUTABLE_STATUS -eq 0 ] && [ $$VALIDATE_STATUS_OFFSETS -eq 0 ] && [ $$VALIDATE_STATUS_OUTPUT -eq 0 ]; then \
			printf "  [SUCCESS] $$config_file (Time: %.3f s)\n" "$$ELAPSED_TIME_SEC"; \
		else \
			printf "  [FAILED] $$config_file (Execute: $$EXECUTABLE_STATUS, Validate Offsets: $$VALIDATE_STATUS_OFFSETS, Validate Output: $$VALIDATE_STATUS_OUTPUT, Time: %.3f s)\n" "$$ELAPSED_TIME_SEC"; \
		fi; \
	done

.PHONY: test
test: $(TEST_CASES)

# Rule: sentinel file depends on input DOT file
$(EVALUATION_CONFIG_DIR)/%/.generated: $(EVALUATION_WORKFLOW_DIR)/%.dot
	@echo "[INFO] Generating configs for workflow: $*"
	@for group in $(EVALUATION_GROUPS); do \
		for json_template in $$(ls $(EVALUATION_TEMPLATE_DIR)/$$group/*.json 2>/dev/null); do \
			BASE_NAME=$$(basename $$json_template .json); \
			CONFIG_DIR=$(EVALUATION_CONFIG_DIR)/$*/$$group/$$BASE_NAME; \
			OUTPUT_DIR=$(EVALUATION_OUTPUT_DIR)/$*/$$group/$$BASE_NAME; \
			LOG_DIR=$(EVALUATION_LOG_DIR)/$*; \
			mkdir -p $$CONFIG_DIR $$OUTPUT_DIR $$LOG_DIR; \
			CONFIG_FILE=$$CONFIG_DIR/config.json; \
			OUTPUT_FILE=$$OUTPUT_DIR/output.yaml; \
			LOG_FILE=$$LOG_DIR/log.txt; \
			$(GENERATE_CONFIG) \
				--template "$$json_template" \
				--output_file "$$CONFIG_FILE" >> "$$LOG_FILE" 2>&1 \
				--params out_file_name="$$OUTPUT_FILE" dag_file="$(EVALUATION_WORKFLOW_DIR)/$*.dot"; \
			GENERATE_STATUS=$$?; \
			if [ $$GENERATE_STATUS -eq 0 ]; then \
				printf "  [SUCCESS] $$CONFIG_FILE\n"; \
			else \
				printf "  [FAILED] $$CONFIG_FILE (Generate: $$GENERATE_STATUS)\n"; \
			fi; \
			sleep $(EVALUATION_SLEEPTIME); \
		done; \
	done
	@touch $@ # Create the sentinel file to indicate completion

%.json: $(EVALUATION_CONFIG_DIR)/%/.generated
	@true # This rule is a placeholder to ensure the directory exists

.PRECIOUS: $(EVALUATION_CONFIG_DIR)/%/.generated

%.yaml: %.json $(EXECUTABLE)
	@echo "[INFO] Running workflow executions for: $*"
	@CONFIG_DIR=$(EVALUATION_CONFIG_DIR)/$*; \
	find $$CONFIG_DIR -name config.json | while read -r CONFIG_FILE; do \
		for repeat in $(shell seq 1 $(EVALUATION_REPEATS)); do \
			LOG_FILE=$$(echo "$$CONFIG_FILE" | sed 's|/config/|/log/|' | sed 's|config.json|'"$$repeat"'.log|'); \
			SRC_FILE=$$(echo "$$CONFIG_FILE" | sed 's|/config/|/output/|' | sed 's|config.json|output.yaml|'); \
			DST_FILE=$$(echo "$$CONFIG_FILE" | sed 's|/config/|/output/|' | sed 's|config.json|'"$$repeat"'.yaml|'); \
			mkdir -p "$$(dirname $$LOG_FILE)"; \
			START_TIME=$$(date +%s.%N); \
			./$(EXECUTABLE) $(RUNTIME_LOG_FLAGS) "$${CONFIG_FILE}" >  "$$LOG_FILE" 2>&1; \
			EXECUTABLE_STATUS=$$?; \
			END_TIME=$$(date +%s.%N); \
			ELAPSED_TIME_SEC=$$(echo "$$END_TIME - $$START_TIME" | bc); \
			mv $$SRC_FILE $$DST_FILE; \
			printf "    Execution time: %.3f s\n" "$$ELAPSED_TIME_SEC" >> "$$LOG_FILE"; \
			$(VALIDATE_OFFSETS) "$${DST_FILE}"  >> "$$LOG_FILE" 2>&1; \
			VALIDATE_STATUS=$$?; \
			if [ $$EXECUTABLE_STATUS -eq 0 ] && [ $$VALIDATE_STATUS -eq 0 ]; then \
				printf "  [SUCCESS] $$CONFIG_FILE (Time: %.3f s)\n" "$$ELAPSED_TIME_SEC"; \
			else \
				printf "  [FAILED] $$CONFIG_FILE (Execute: $$EXECUTABLE_STATUS, Validate: $$VALIDATE_STATUS, Time: %.3f s)\n" "$$ELAPSED_TIME_SEC"; \
			fi; \
			sleep $(EVALUATION_SLEEPTIME); \
		done; \
	done

.PRECIOUS: $(EVALUATION_SLURM_DIR)/%.slurm

$(EVALUATION_SLURM_DIR)/%.slurm: $(EVALUATION_CONFIG_DIR)/%/.generated $(EXECUTABLE)
	@echo "[INFO] Generating SLURM job for: $*"
	@mkdir -p "$(EVALUATION_SLURM_DIR)"
	@CONFIG_DIR=$(EVALUATION_CONFIG_DIR)/$*; \
	find $$CONFIG_DIR -name config.json | while read -r CONFIG_FILE; do \
		SLURM_FILE=$$(echo "$$CONFIG_FILE" | sed 's|/config/|/slurm/|' | sed 's|config.json|submit.sbatch|'); \
		$(GENERATE_SUBMIT) --job "$*" \
			--config_file "$$CONFIG_DIR" \
			--execute_command "./$(EXECUTABLE) $(RUNTIME_LOG_FLAGS) $${CONFIG_FILE}" \
			--validate_command "$(VALIDATE_OFFSETS)" \
			--repeats $(EVALUATION_REPEATS) \
			--output "$$SLURM_FILE" > /dev/null 2>&1; \
		GENERATE_STATUS=$$?; \
		if [ $$GENERATE_STATUS -eq 0 ]; then \
			printf "  [SUCCESS] $$SLURM_FILE\n"; \
		else \
			printf "  [FAILED] $$SLURM_FILE (Generate: $$GENERATE_STATUS)\n"; \
		fi; \
	done
	@touch $@ # Create the sentinel file to indicate completion
	
# Redirect target (so `make NAME.slurm` works)
%.slurm: $(EVALUATION_SLURM_DIR)/%.slurm
	@true
