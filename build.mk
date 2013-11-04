
SINGLE_TARGET+=$(TESTS)
OBJ=$(SRC:.c=.o)

all: $(TARGET) $(TARGETLIB) $(SINGLE_TARGET)

$(OBJ): $(SRC) 
	$(CC) $(CFLAGS) -c -o $@ $<

$(SINGLE_TARGET): $(SINGLE_TARGET:=.c) $(TARGET_DEP)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

$(TARGET): $(OBJ) $(TARGET_DEP)
	$(CC) -o $(TARGET) $(LDFLAGS) $(OBJ) $(LIBS)

$(TARGETLIB): $(OBJ) $(TARGET_DEP)
	$(AR) $(TARGETLIB) $(OBJ) $(TARGET_DEP)  
	
clean:
	$(RM) $(OBJ) $(TARGET) $(SINGLE_TARGET) $(TARGETLIB)  

distclean:
	$(RM) $(OBJ) $(TARGET) $(SINGLE_TARGET) $(TARGETLIB) $(DEP_FILES) \
		*.core .*.swp

