	section	.text,"ax",@progbits
	assume	adl = 1
	section	.text,"ax",@progbits
	public	_f
_f:
	call	__frameset0
	ld	iy, (ix + 6)
	ld.sis	hl, 0
	ld	(iy + 127), l
	ld	(iy + 128), h
	pop	ix
	ret
	section	.text,"ax",@progbits

	ident	"clang version 15.0.0 (https://github.com/jacobly0/llvm-project 6c61664110f888c0285ae4c48b150c9a7a4361bb)"
	extern	__Unwind_SjLj_Register
	extern	__Unwind_SjLj_Unregister
	extern	__frameset0
