#

nop:

lazymove: lazymove.c
	gcc -o lazymove lazymove.c

lazycopy: lazymove
	ln -v -snf lazymove lazycopy
