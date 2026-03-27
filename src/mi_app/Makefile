# mi_app — Makefile
#
# Genera un único app.elf usando la infraestructura de src/sdk.
#
# Uso:
#   make          Compila mi_app → app.elf
#   make clean    Elimina artefactos de compilación

SDK_DIR = ../sdk
SRCS     = $(wildcard *.c)
SRCS_CXX = $(wildcard *.cpp)

include $(SDK_DIR)/sdk.mk

# Regla explícita para el CRT (la regla de patrón no cruza directorios)
$(CRT_DIR)/start.o: $(CRT_DIR)/start.S
	$(AS) $(ASFLAGS) -c -o $@ $<

all: app.elf
	$(SIZE) $<

clean: sdk-clean

.PHONY: all clean
