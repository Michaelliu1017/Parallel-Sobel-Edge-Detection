CXX = g++
CXXFLAGS = -O2 -std=c++17 -fopenmp
LIBS = -lm
TARGET = sobel

all: $(TARGET)
	@./$(TARGET) 2>/dev/null || true

$(TARGET): sobel.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) sobel.cpp $(LIBS)

clean:
	rm -f $(TARGET)
