FROM fedora

RUN dnf -y install boost-program-options spdlog acl \
    && dnf clean all
