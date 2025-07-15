#==============================
#    Build and Testing Logic
#==============================

# Compiler and Flags
CXX := g++
CXXFLAGS := -Wall -O2 -fno-prefetch-loop-arrays -Iinclude #-g #-DNLOG # Uncomment for debugging: -fsanitize=address
LIBS := -lsimgrid -lhwloc

SRC_DIR := src
INC_DIR := include
OBJ_DIR := obj
BIN_DIR := bin
TEST_DIR := tests

# Source and object files
SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SOURCES))
TARGET := $(BIN_DIR)/nflows

# Validation Scripts
SCRIPTS_DIR := ./scripts
PYTHON_EXEC := $(SCRIPTS_DIR)/.env/bin/python3

VALIDATE_OFFSETS := $(PYTHON_EXEC) $(SCRIPTS_DIR)/validate_offsets.py
VALIDATE_OUTPUT := $(PYTHON_EXEC) $(SCRIPTS_DIR)/validate_output.py

TEST_DIR := ./tests
TEST_CONFIG_DIR := $(TEST_DIR)/config
TEST_EXPECTED_DIR := $(TEST_DIR)/expected

TEST_LOG_DIR := $(TEST_DIR)/log
TEST_OUTPUT_DIR := $(TEST_DIR)/output

# Test Targets
TEST_CASES := $(patsubst $(TEST_CONFIG_DIR)/%,%,$(wildcard $(TEST_CONFIG_DIR)/test_*))

# Build Target
$(TARGET): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

# Compile Source to Object
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Test Execution
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
		./$(TARGET) $(RUNTIME_LOG_FLAGS) $$config_file > "$$LOG_FILE" 2>&1; \
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

# Clean Rule
clean:
	@echo "[INFO] Cleaning build files..."
	@rm -rf $(OBJ_DIR) $(BIN_DIR) $(TEST_OUTPUT_DIR) $(TEST_LOG_DIR)