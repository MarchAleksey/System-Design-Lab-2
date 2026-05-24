FROM ghcr.io/userver-framework/ubuntu-22.04-userver-pg-dev:latest

ENV CCACHE_DIR=/home/user/lab2/.ccache \
  CORES_DIR=/cores \
  PREFIX=~/.local \
  SHELL=/bin/bash

USER user

WORKDIR /home/user/lab2

COPY . .

RUN make build-release

EXPOSE 8080

CMD ["make", "start-release"]
