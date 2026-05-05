FROM gcc:13

WORKDIR /workspace

COPY . .

RUN ./build.sh

CMD ["./test.sh"]
