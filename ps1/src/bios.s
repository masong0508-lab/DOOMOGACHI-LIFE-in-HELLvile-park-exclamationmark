# bios.s - BIOS calls PSn00bSDK does not wrap (owner: ps1).
	.set noreorder
	.set noat
	.section .text.bios_rename, "ax", @progbits
	.global bios_rename
	.type bios_rename, @function
# int bios_rename(const char *old, const char *new)   B(44h)
bios_rename:
	li	$t2, 0xb0
	jr	$t2
	li	$t1, 0x44
