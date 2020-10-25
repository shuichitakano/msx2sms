
all:	bios_patch.bin	bios_emu.lst bios_patcher


bios_patch.bin:	bios_patch.ihx
	ihx2sms bios_patch.ihx bios_patch.bin

bios_patch.ihx:	bios_patch.rel
	sdcc -mz80 --no-std-crt0 bios_patch.rel

bios_patch.rel:	bios_patch.s
	sdasz80 -l -o bios_patch.s


bios_emu.lst:	bios_emu.c
	sdcc -mz80 bios_emu.c


bios_patcher:	bios_patcher.cxx
	g++ -g --std=c++11 bios_patcher.cxx -o bios_patcher
