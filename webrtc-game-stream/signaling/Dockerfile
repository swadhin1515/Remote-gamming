FROM node:20-alpine
WORKDIR /app
COPY server.js .
RUN npm init -y && npm i ws
EXPOSE 9000
CMD ["node", "server.js"]
