obj-m += nxp_simtemp.o

all: build dt
	echo Build DT overlay and simtemp kernel module

build:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules

dt: nxp_simtemp.dts
	dtc -@ -I dts -O dtb -o nxp_simtemp.dtbo nxp_simtemp.dts

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean
	rm -rf nxp_simtemp.dtbo

sim_enabled:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules EXTRA_CFLAGS="-DSIM"
	
