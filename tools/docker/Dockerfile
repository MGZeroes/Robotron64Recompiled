# Linux build image for the parts of the pipeline that need Linux (ADR 003):
# the decomp ELF build (IDO 5.3 recomp, binutils-mips) and its spimdisasm analysis,
# n64sym, and N64Recomp/RSPRecomp (built from the repo's submodule at run time).
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        python3 python3-venv python3-pip \
        make git ca-certificates \
        binutils-mips-linux-gnu \
        cmake ninja-build g++ \
    && rm -rf /var/lib/apt/lists/*

# n64sym (shygoo, MIT): signature-based libultra identification. Pinned; see ADR 004.
ARG N64SYM_COMMIT=ccf4600f3389f1a84bde23339225cf372fdf7712
RUN git clone https://github.com/shygoo/n64sym.git /opt/n64sym \
    && git -C /opt/n64sym checkout ${N64SYM_COMMIT} \
    && make -C /opt/n64sym \
    && find /opt/n64sym -maxdepth 2 -type f -name n64sym -perm -u+x -exec ln -s {} /usr/local/bin/n64sym \; \
    && n64sym 2>&1 | head -1

WORKDIR /work
