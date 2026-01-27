make: src/measurement.o src/msr_reader.o src/parallel.o $(file)
	$(CXX) $(CXXFLAGS) src/measurement.o src/msr_reader.o src/parallel.o $(file) -o energy-effi

run: make
	sudo ./energy-effi $(args)

src/measurement.o: src/measurement.cpp src/measurement.hpp
	$(CXX) $(CXXFLAGS) -c src/measurement.cpp -o src/measurement.o

src/msr_reader.o: src/msr_reader.c src/msr_reader.h
	$(CXX) $(CXXFLAGS) -c src/msr_reader.c -o src/msr_reader.o