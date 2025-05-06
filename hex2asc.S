  # hexmain.S
  # Written 2015-09-04 by F Lundevall
  # Copyright abandonded - this file is in the public domain.

	.text
	.globl hex2asc

hex2asc:
	li	a0, 15		# test number (from 0 to 15)
	
	addi    sp,sp,-4
	sw      ra,0(sp)
	
	jal	hexasc		# call hexasc
	
	li	a7, 11	# write a0 to stdout
	ecall

	lw      ra,0(sp)
	addi    sp,sp,4
	jr      ra	

  # You can write your own code for hexasc here
  #
hexasc:
	andi 	a0, a0, 0xF	# let a0 only keep the 4 lsb (since 4 bits is at most 15, a0 will always be in 0-15)
	addi 	t0, zero, 9
	ble 	a0, t0, return	# don't add letter base if is 0-9
	addi 	a0, a0, 7	# add letter base
return: 
	addi 	a0, a0, 0x30 	# add ascii base
	jr 	ra
	