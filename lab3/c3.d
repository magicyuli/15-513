
c3.o:     file format elf64-x86-64


Disassembly of section .text:

0000000000000000 <.text>:
   0:	48 c7 c7 08 a9 60 55 	mov    $0x5560a908,%rdi
   7:	48 c7 c0 69 1c 40 00 	mov    $0x401c69,%rax
   e:	48 89 04 24          	mov    %rax,(%rsp)
  12:	48 b8 33 66 63 35 61 	mov    $0x3232316135636633,%rax
  19:	31 32 32 
  1c:	48 89 07             	mov    %rax,(%rdi)
  1f:	c3                   	retq   
