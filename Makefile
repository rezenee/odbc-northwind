EXE = a4
OBJECTS = a4.o
CFLAGS = -g -lodbc -lm
$(EXE) : $(OBJECTS) 
	gcc -o $(EXE)  $(OBJECTS) $(CFLAGS)

$(OBJECTS) : 
.PHONY : clean
clean :
	rm $(EXE) $(OBJECTS)
