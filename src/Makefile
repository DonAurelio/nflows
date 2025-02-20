CXX = g++
CXXFLAGS = -Wall -O2 # -fsanitize=address
LIBS = -lsimgrid -lhwloc

SRC = common.cpp hardware.cpp scheduler_base.cpp scheduler_fifo.cpp \
      scheduler_eft.cpp scheduler_heft.cpp scheduler_min_min.cpp \
      mapper_base.cpp mapper_bare_metal.cpp mapper_simulation.cpp \
	  runtime.cpp main.cpp

OBJ = $(SRC:.cpp=.o)

TARGET = scheduler
CONFIG_FILE = ../data_eval/config/config.json

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) $(CONFIG_FILE)

clean:
	rm -f $(OBJ) $(TARGET) *.yml
