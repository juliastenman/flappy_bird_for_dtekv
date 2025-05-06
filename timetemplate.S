# timetemplate.S
# Written 2015 by F Lundevall
# Ported 2024/06 by W Szczerek (from MIPS to RISC-V)
# Copyright abandonded - this file is in the public domain.

#############################################################
# Choose the macro syntax for RARS or DTEK-V board.         #
# The syntax for RARS is probably due to its MIPS heritage. #
#############################################################
###################
# Macros for RARS #
###################
#.macro	PUSH (%reg)
#	addi	sp,sp,-4
#	sw	%reg,0(sp) 
#.end_macro
#
#.macro	POP (%reg)
#	lw	%reg,0(sp)
#	addi	sp,sp,4
#.end_macro
###################
# Macros for DTEK #
###################
.macro	PUSH reg
	addi sp,sp,-4
	sw \reg,0(sp) 
.endm

.macro	POP reg
	lw	\reg,0(sp)
	addi	sp,sp,4
.endm
#############################################################

	.data
	.align 2
mytime:	.word 	0x5957
timstr:	.asciz 	"text more text lots of text\0"
	.text
	.globl timetemplate, tick, time2string, delay, display_string

# Function for displaying a string with a newline at the end	
display_string:	
	li a7,4
	ecall
	li a0, 10
	li a7,11
	ecall
	jr ra
	
timetemplate:
	la	a0, timstr
	jal     display_string
	
	# wait a little
	li	a0, 2		# ms
	jal	delay
	
	# call tick
	la 	a0, mytime
	jal	tick
	
	# call your function time2string
	la	a0, timstr
	la	t0, mytime
	lw	a1, 0(t0)
	jal	time2string
	
	# go back and do it all again
	j	timetemplate

	
# tick: update time pointed to by $a0
tick:	lw	t0, 0(a0)	# get time
	addi	t0, t0, 1	# increase
	andi	t1, t0, 0xf	# check lowest digit
	sltiu	t2, t1, 0xa	# if digit < a, okay
	bnez	t2, tiend
	addi	t0, t0, 0x6	# adjust lowest digit
	
	andi	t1, t0, 0xf0	# check next digit
	sltiu	t2, t1, 0x60	# if digit < 6, okay
	bnez	t2, tiend
	addi	t0, t0, 0xa0	# adjust digit
	
	li	t3, 0xF
	slli	t3, t3, 0x8
	and	t1, t0, t3	# check minute digit
	addi	t3, x0, 0xA
	slli	t3, t3, 0x8
	slt	t2, t1, t3	# if digit < a, okay
	bnez	t2, tiend
	addi	t0, t0, 0x600	# adjust digit - this one's okay, it's lower than 0x7FF 
	
	li	t3, 0xF
	slli	t3, t3, 0xC
	and	t1, t0, t3	# check last digit
	addi	t3, x0, 0x6
	slli	t3, t3, 0xC
	slt	t2, t1, t3	# if digit < 6, okay
	bnez	t2, tiend
	
	li	t3, 0xA
	slli	t3, t3, 0xC
	add	t0, t0, t3	# adjust last digit
tiend:	sw	t0,0(a0)	# save updated result
	jr	ra		# return

#########################################################
# Place for your functions: time2string, hex2asc, delay.#
#########################################################

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

hexasc:
	andi 	a0, a0, 0xF	# let a0 only keep the 4 lsb (since 4 bits is at most 15, a0 will always be in 0-15)
	addi 	t0, zero, 9
	ble 	a0, t0, return	# don't add letter base if is 0-9
	addi 	a0, a0, 7	# add letter base
return: 
	addi 	a0, a0, 0x30 	# add ascii base
	jr 	ra
	
delay:
	# a0=ms, 
	# a1=ITERATIONS (was 4711),
	# a2=i
	ble 	a0, zero, timeout	# 0 or less ms left
	li	a1, 15000			# constant number of loops per ms (ITERATIONS)
	addi 	a0, a0, -1		# ms--
	li 	a2, -1			# i=-1 (increments to 0 before running for)
for:
	addi 	a2, a2, 1		# i++
	blt	a2, a1, for		# if i < ITERATIONS: for
	j	delay
timeout:
	jr 	ra
	
time2string:
	# save nontemporary variables (callee-saved)
	PUSH 	ra
	PUSH 	s0		# store s0 in stack
	PUSH 	s1		# store s1 in stack
	
	li 	t0, 0xFFFF
	and 	a1, a1, t0 	# only keep the 16 lsb (unnecessary but just in case)
	add 	s0, a0, zero	# store memory address from a0 in s0
	
	# get ascii rep of first second
	andi 	a0, a1, 0xF	# store first sec in lsb in a0
	srli	s1, a1, 4	# store rest of time in s1
	li	t0, 2
	jal 	hexasc
	sb 	a0, 4(s0) 	# store first sec byte in s0 memory loc
	sb 	zero, 5(s0)	# store null-byte

	# get ascii rep of ten-second
	andi 	a0, s1, 0xF	# store tenth second in a0
	srli	s1, s1, 4	# store rest of time in s1
	jal 	hexasc
	sb 	a0, 3(s0)	# store tenth second byte in s0 memory loc
	
	# get colon
	li 	t0, 0x3a 	# load colon 
	sb 	t0, 2(s0)	# store colon in s0 memory loc.
	
	# get ascii rep of single min
	andi 	a0, s1, 0xF	# store first min in lsb in a0
	srli 	s1, s1, 4	# store rest of time in s1
	jal 	hexasc
	sb 	a0, 1(s0)
	
	# get ascii rep of ten min
	andi 	a0, s1, 0xF	# store ten min in lsb in a0
	jal 	hexasc
	sb 	a0, (s0)
	
	# restore stack
	POP 	s1		# put back s1
	POP 	s0 		# put back s0
	
	POP 	ra
	jr 	ra
