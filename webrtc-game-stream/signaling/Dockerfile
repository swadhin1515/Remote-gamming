FROM node:20-alpine

WORKDIR /app

# Copy package files
COPY package*.json ./

# Copy pre-downloaded ws package
COPY ws-8.16.0.tgz ./

# Install dependencies from local tarball
RUN npm install ws-8.16.0.tgz --production

# Copy application code
COPY server.js .

EXPOSE 9000

CMD ["node", "server.js"]
