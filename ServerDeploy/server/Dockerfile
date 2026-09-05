FROM gcr.io/distroless/cc-debian10:nonroot

COPY --chown=nonroot:nonroot ./server /home/nonroot/server

ENTRYPOINT [ "/home/nonroot/server/Crunch/Binaries/Linux/CrunchServer", "Crunch"]
