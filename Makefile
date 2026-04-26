CC = cl.exe
LINKER = link.exe

CFLAGS = /nologo /Zi /EHsc /Iinclude /D_CRT_SECURE_NO_WARNINGS /Fd$(OBJ_DIR)\vc140.pdb
LFLAGS = /nologo /DEBUG

SRC_DIR = src
BIN_DIR = bin
OBJ_DIR = build
PAYLOAD_DIR = payload

INJECTOR_OBJS = $(OBJ_DIR)\main.obj $(OBJ_DIR)\pe_parser.obj $(OBJ_DIR)\hollowing.obj
TARGET = $(BIN_DIR)\injector.exe

PAYLOAD_SRC = $(PAYLOAD_DIR)\dummy_msg.c
PAYLOAD_TARGET = $(BIN_DIR)\payload.exe

all: setup $(TARGET) payload

setup:
	@if not exist $(BIN_DIR) mkdir $(BIN_DIR)
	@if not exist $(OBJ_DIR) mkdir $(OBJ_DIR)

{src}.c{build}.obj:
	$(CC) $(CFLAGS) /c $< /Fo$@

$(TARGET): $(INJECTOR_OBJS)
	$(LINKER) $(LFLAGS) /OUT:$@ $(INJECTOR_OBJS) /PDB:$(OBJ_DIR)\injector.pdb /ILK:$(OBJ_DIR)\injector.ilk

payload: setup
	$(CC) $(CFLAGS) $(PAYLOAD_SRC) /Fo$(OBJ_DIR)\dummy_msg.obj /Fe:$(PAYLOAD_TARGET) \
	/link /SUBSYSTEM:WINDOWS user32.lib \
	/PDB:$(OBJ_DIR)\payload.pdb /ILK:$(OBJ_DIR)\payload.ilk

clean:
	@if exist $(OBJ_DIR) rd /s /q $(OBJ_DIR)
	@if exist $(BIN_DIR) rd /s /q $(BIN_DIR)