FROM nginx:1.27-alpine

# Remove default config
RUN rm /etc/nginx/conf.d/default.conf

# Custom nginx config
COPY nginx.conf /etc/nginx/conf.d/mancala.conf

# Static files
COPY html/ /usr/share/nginx/html/

EXPOSE 80
