# Spam requests
# https://unix.stackexchange.com/questions/230673/how-to-generate-a-random-string

for i in $(seq 1 100); do
    curl --silent \
    http://localhost:8080/$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c$(($RANDOM % 15))) \
    http://localhost:8080/$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c$(($RANDOM % 15))) \
    http://localhost:8080/$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c$(($RANDOM % 15))) \
    http://localhost:8080/$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c$(($RANDOM % 15))) \
    http://localhost:8080/$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c$(($RANDOM % 15))) \
    http://localhost:8080/$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c$(($RANDOM % 15))) \
    http://localhost:8080/$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c$(($RANDOM % 15))) \
    http://localhost:8080/$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c$(($RANDOM % 15))) \
    http://localhost:8080/$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c$(($RANDOM % 15))) \
    http://localhost:8080/$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c$(($RANDOM % 15))) > /dev/null
done

