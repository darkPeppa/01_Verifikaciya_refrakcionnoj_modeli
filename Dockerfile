# Task 01 "Verifikaciya refrakcionnoj modeli" (verification of the X-ray refraction model)
# + stage 2 data (interaction physics in the SU-8 lenses).
#
# Complete, self-contained environment: Geant4 11.4.2 with all data sets, C++ compiler,
# CMake, Python + numpy + matplotlib are installed INSIDE the image. The host needs only
# Docker (Windows / Linux / macOS, x86-64 or ARM64). The program is compiled and its unit
# tests are run while the image is built.
FROM mambaorg/micromamba:1.5.10

# ---------- 1. software environment, pinned to the exact validated package builds ----------
ARG TARGETARCH
COPY --chown=$MAMBA_USER:$MAMBA_USER docker/ /tmp/env/
RUN set -eux; \
    case "${TARGETARCH:-$(uname -m)}" in \
      amd64|x86_64)   LOCK=/tmp/env/conda-lock-linux-64.txt ;; \
      arm64|aarch64)  LOCK=/tmp/env/conda-lock-linux-aarch64.txt ;; \
      *)              LOCK= ;; \
    esac; \
    if [ -n "$LOCK" ] && [ -f "$LOCK" ]; then \
      echo "Installing the exact package set from $LOCK"; \
      micromamba install -y -n base --file "$LOCK"; \
    else \
      echo "No lock file for this architecture: solving docker/environment.yml"; \
      micromamba install -y -n base -f /tmp/env/environment.yml; \
    fi; \
    micromamba clean -a -y

# Following RUN steps run inside the activated environment (Geant4 data-set variables set).
ARG MAMBA_DOCKERFILE_ACTIVATE=1
ENV MPLBACKEND=Agg \
    BUILD_DIR=/work/build \
    OUT=/work/out

# ---------- 2. compile the C++ code and run the unit tests ----------
USER root
RUN mkdir -p /work && chown "$MAMBA_USER:$MAMBA_USER" /work
USER $MAMBA_USER
WORKDIR /work
COPY --chown=$MAMBA_USER:$MAMBA_USER CMakeLists.txt xraycrl.cc /work/
COPY --chown=$MAMBA_USER:$MAMBA_USER include /work/include
COPY --chown=$MAMBA_USER:$MAMBA_USER src /work/src
COPY --chown=$MAMBA_USER:$MAMBA_USER tests /work/tests
RUN set -eux; \
    cmake -S /work -B /work/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_AR="$(command -v gcc-ar)" -DCMAKE_RANLIB="$(command -v gcc-ranlib)"; \
    cmake --build /work/build -j "$(nproc)"; \
    /work/build/unit_tests

# ---------- 3. configurations, analysis, poster generator, documentation ----------
COPY --chown=$MAMBA_USER:$MAMBA_USER . /work/
# tolerate Windows line endings if files were edited on Windows
RUN find /work/scripts /work/tests /work/analysis /work/poster /work/macros /work/config -type f \
        \( -name '*.sh' -o -name '*.py' -o -name '*.mac' -o -name '*.json' -o -name '*.csv' \) \
        -exec sed -i 's/\r$//' {} + \
    && chmod +x /work/scripts/*.sh

ENTRYPOINT ["/usr/local/bin/_entrypoint.sh", "bash", "/work/scripts/entry.sh"]
CMD ["all"]
