CC = cl.exe
LINKER = link.exe

CFLAGS = /nologo /Zi /EHsc /Iinclude /D_CRT_SECURE_NO_WARNINGS

LFLAGS = /nologo /DEBUG

SRC_DIR = src
BIN_DIR = bin
OBJ_DIR = build

OBJS = $(OBJ_DIR)\main.obj

TARGET = $(BIN_DIR)\ProcessMorph.exe

all: setup $(TARGET)

setup:
	@if not exist $(BIN_DIR) mkdir $(BIN_DIR)
	@if not exist $(OBJ_DIR) mkdir $(OBJ_DIR)

$(TARGET): $(OBJS)
	$(LINKER) $(LFLAGS) /OUT:$@ $(OBJS)

$(OBJ_DIR)\main.obj: $(SRC_DIR)\main.c
	$(CC) $(CFLAGS) /c $(SRC_DIR)\main.c /Fo$@

clean:
	@if exist $(OBJ_DIR) rd /s /q $(OBJ_DIR)
	@if exist $(BIN_DIR) rd /s /q $(BIN_DIR)