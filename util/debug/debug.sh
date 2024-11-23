gdb --quiet --init-command="$(pwd)/debug.gdb" \
    --args /usr/local/nginx/sbin/nginx -c $(pwd)/debug.conf -p $(pwd)

for d in $(ls | grep "_temp"); do
   rmdir $d
done

