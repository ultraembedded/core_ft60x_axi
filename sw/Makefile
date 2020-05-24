COMMON_SRC = ftdi_axi_driver.cpp ftdi_ft60x.cpp
CFLAGS     = -Ilinux-x86_64
LFLAGS     = -Llinux-x86_64
LIBS       = -l:libftd3xx.so

TARGETS    = peek poke load verify check gpio_wr gpio_rd
all: $(TARGETS)

$(TARGETS):
	g++ -o $@ $(CFLAGS) $(LFLAGS) $@.cpp $(COMMON_SRC) $(LIBS)

clean:
	-rm -rf $(TARGETS)