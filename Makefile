FILE_DIR_0 = arrays
DEPS = server.h chat.h $(FILE_DIR_0)/active-conn-array.h $(FILE_DIR_0)/send-array.h $(FILE_DIR_0)/int-array.h
FILE = server.c chat.c $(FILE_DIR_0)/active-conn-array.c $(FILE_DIR_0)/send-array.c $(FILE_DIR_0)/int-array.c

a.out : $(FILE) $(DEPS)
	gcc -Wall $(FILE)