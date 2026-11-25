# syntax=docker/dockerfile:1.7
# Commit image for gr-ieee802-11 (GNU Radio 3.10 OOT module). Built on the org base.
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
ARG SOURCE_DATE_EPOCH
ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

# GNU Radio 3.10 runtime/blocks/filter/fft/digital/analog + VOLK and the module's build deps.
# apt GNU Radio stays uninstrumented; build.sh instruments this module's own lib/*.cc.
USER root
RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
    apt-get update && apt-get install -yq --no-install-recommends \
        gnuradio-dev cmake make pkg-config libboost-dev libboost-thread-dev \
        libboost-program-options-dev libspdlog-dev libfmt-dev libgmp-dev \
        libvolk-dev liborc-0.4-dev \
 && rm -rf /var/lib/apt/lists/*
USER mayhem

WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
