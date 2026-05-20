FROM ghcr.io/userver-framework/ubuntu-24.04-userver:latest

WORKDIR /service

COPY . .

RUN cmake --preset release && \
    cmake --build build-release -j $(nproc) --target messenger_service

EXPOSE 8080

ENV PREFIX=/service

CMD ["./build-release/messenger_service", \
     "--config", "/service/configs/static_config.yaml", \
     "--config_vars", "/service/configs/config_vars.yaml"]
