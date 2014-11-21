FROM ubuntu

RUN apt-get update -y
RUN apt-get install git build-essential -y
COPY . /code
WORKDIR /code

RUN make
RUN make net_link

ENV PATH $PATH:/code/bin
