COMMON_INCLUDE_DIR     := .
COMMON_SOURCE_DIR      := .

COMMON_INCLUDES        := $(COMMON_INCLUDE_DIR)/macroasstring.h
COMMON_SOURCES         := 
COMMON_LIBS            := wiringPi
COMMON_DEFINES         := 

PIDISKLEDS_SOURCES     := PiDiskLeds.c $(COMMON_SOURCES)
PINETLEDS_SOURCES      := PiNetLeds.c $(COMMON_SOURCES)

CC                      = gcc
CFLAGS                  = -std=gnu11 -o $@ -I$(COMMON_INCLUDE_DIR) -l$(COMMON_LIBS) -Wall -O3


PiDiskLeds : $(PIDISLEDS_SOURCES) pidiskleds.h pidiskledsstrings.h $(COMMON_INCLUDES)
	$(CC) $(PIDISKLEDS_SOURCES) $(CFLAGS)
    
PiNetLeds  : $(PINETLEDS_SOURCES) pinetleds.h pinetledsstrings.h $(COMMON_INCLUDES)
	$(CC) $(PINETLEDS_SOURCES) $(CFLAGS)

.PHONY: all
all: PiDiskLeds PiNetLeds

.PHONY: clean	
clean:
	rm -f PiDiskLeds PiNetLeds
