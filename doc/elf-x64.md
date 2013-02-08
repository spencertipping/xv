XV x86-64 ELF header | Spencer Tipping
Licensed under the terms of the MIT source code license

# Introduction

XV is written as a single-section ELF file for compactness. It's important to
keep the memory footprint small because XV may need to relocate itself if the
process attempts to map the memory it occupies. (XV being smaller both
decreases the likelihood that this will happen, and it decreases the cost of
relocating.)

We map the whole ELF image into memory and reuse the bytes in the header to
store global data.

    :[::image-base = 0x200000]
    :[sub a { shift(@_) + :image-base }]

    ::elf-ehdr-begin
    ::xv-sigstate-o         7f 'ELF  02010100                       # e_ident
    ::xv-sigill-o           00000000 00000000                       # e_ident
    ::xv-sigsegv-o          02003e00 01000000

    ::xv-pagemap-o          :8[La:xv-main]                          # entry point
    ::xv-sysvirt-o          :8[L:phdr-begin]                        # PH offset
    ::xv-lock               :8[L0]                                  # SH offset

                            00000000
                            :2[L :elf-ehdr-end - :elf-ehdr-begin]
                            :2[L :elf-phdr-end - :elf-phdr-begin]

                            0100 0000 0000 0000                     # phnum, SH
    ::elf-ehdr-end

    ::elf-phdr-begin
                            0100 0000 0700 0000                     # type, flags
                            0000 0000 0000 0000                     # offset
                            :8[La0]                                 # vaddr
                            :8[L0]                                  # paddr
                            :8[L :image-end - :image-begin]         # filesz
                            :8[L :image-end - :image-begin]         # memsz
                            :8[L0x1000]                             # align
    ::elf-phdr-end