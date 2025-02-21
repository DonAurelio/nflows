CXX = g++
CXXFLAGS = -Wall -O2 -DNLOG # -fsanitize=address
LIBS = -lsimgrid -lhwloc

SRC = common.cpp hardware.cpp scheduler_base.cpp scheduler_fifo.cpp \
      scheduler_eft.cpp scheduler_heft.cpp scheduler_min_min.cpp \
      mapper_base.cpp mapper_bare_metal.cpp mapper_simulation.cpp \
      runtime.cpp main.cpp

OBJ = $(SRC:.cpp=.o)

TARGET = scheduler
CONFIG_DIR = ./tests/config
CONFIG_FILES = $(wildcard $(CONFIG_DIR)/*.json)
YAML_OUTPUT = output.yaml
VALIDATOR_SCRIPT = ./python/validate_offsets.py
CPP_GENERATOR = scheduler

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) $(CONFIG_DIR)/1_min_min_sim_redis_tasks_1.json

clean:
	rm -f $(OBJ) $(TARGET) *.yaml

test: $(TARGET)
	@for json in $(CONFIG_FILES); do \
		echo "Running test for $$json"; \
		./$(TARGET) $$json $(YAML_OUTPUT); \
		python3 $(VALIDATOR_SCRIPT) $(YAML_OUTPUT); \
	done
