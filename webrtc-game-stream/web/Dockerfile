FROM mirror.gcr.io/library/nginx:alpine
COPY nginx.conf /etc/nginx/conf.d/default.conf
COPY index.html /usr/share/nginx/html/index.html
COPY client.js /usr/share/nginx/html/client.js
COPY index-scaler.html /usr/share/nginx/html/index-scaler.html
COPY client-scaler.js /usr/share/nginx/html/client-scaler.js
COPY index-auto.html /usr/share/nginx/html/index-auto.html
COPY client-auto.js /usr/share/nginx/html/client-auto.js