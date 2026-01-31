make: ./out/measurement.o ./out/msr_reader.o ./out/parallel.o $(file)
	$(CXX) $(CXXFLAGS) -O0 ./out/measurement.o ./out/msr_reader.o ./out/parallel.o $(file) -o ./out/$(basename $(notdir $(file)))

run: make
	sudo ./out/$(basename $(notdir $(file))) $(args)
	.venv/bin/python plot.py results/$(basename $(notdir $(file)))_$(args).csv

./out/parallel.o: src/parallel.cpp src/parallel.hpp
	$(CXX) $(CXXFLAGS) -c src/parallel.cpp -o ./out/parallel.o

./out/measurement.o: src/measurement.cpp src/measurement.hpp
	$(CXX) $(CXXFLAGS) -c src/measurement.cpp -o ./out/measurement.o

./out/msr_reader.o: src/msr_reader.c src/msr_reader.h
	$(CXX) $(CXXFLAGS) -c src/msr_reader.c -o ./out/msr_reader.o

clean:
	rm -f ./out/*