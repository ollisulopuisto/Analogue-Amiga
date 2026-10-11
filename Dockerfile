FROM debian:bookworm-slim

RUN apt-get update -qq && \
    apt-get install -y -qq gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf make clang git && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src

CMD ["bash", "-c", "clang++ -std=c++14 tests/test_amiga_shortcuts.cpp -o test_shortcuts && ./test_shortcuts && cd src/MPUBIOS/app && make clean && make GCC_PREFIX=riscv64-unknown-elf- COMPILER=g++"]
