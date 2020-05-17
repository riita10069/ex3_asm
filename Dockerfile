FROM ubuntu:latest 
COPY . .
WORKDIR /asm

RUN apt-get update -y && \
    apt-get upgrade -y && \
    apt-get dist-upgrade -y && \
    apt-get install make curl git sudo cron -y && \
    apt-get -y install build-essential

RUN make
