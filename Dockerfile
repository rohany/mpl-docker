FROM ubuntu:18.10

RUN apt-get update && apt-get install -y mlton vim

COPY mpl-build /mpl/
ENV PATH=$PATH:/mpl/bin/
