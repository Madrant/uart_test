C_FILES_UART = uart.c uart_options.c

C_FILES = uart_test.c $(C_FILES_UART) utils.c

ELF_FILE = uart_test

#D_ENABLE_DEBUG = -DD_DEBUG -DUART_DEBUG
D_ENABLE_DEBUG = -DUART_DEBUG

MORE_PARAMS =

debug:
		$(CROSS_COMPILE)gcc -std=gnu99 -O2 $(D_ENABLE_DEBUG) $(MORE_PARAMS) $(C_FILES) -o $(ELF_FILE)_debug

debug_noprintf:
		$(CROSS_COMPILE)gcc -std=gnu99 -O2 $(MORE_PARAMS) $(C_FILES) -o $(ELF_FILE)_debug_noprintf

release:
		$(CROSS_COMPILE)gcc -std=gnu99 -DNDEBUG -O2 $(MORE_PARAMS) $(C_FILES) -o $(ELF_FILE)
		$(CROSS_COMPILE)strip -s $(ELF_FILE)

debug: debug debug_noprintf

release: release

all: debug release

clean:
		rm -f $(ELF_FILE)
		rm -f $(ELF_FILE)_debug
		rm -f $(ELF_FILE)_debug_noprintf

