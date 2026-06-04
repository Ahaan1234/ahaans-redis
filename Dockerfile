FROM ubuntu:24.04
RUN apt-get update && apt-get install -y g++ make strace gdb
WORKDIR /code