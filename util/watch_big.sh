# Watch error.log and stop Nginx when the file reaches 1 MB or more

while [ 1 ]; do
   ls -l --human-readable /tmp/error.log | awk '{print $5}' | grep "M"
   if [ $? -eq 0 ]; then
      sudo /usr/local/nginx/sbin/nginx -s stop
      break
   fi
   sleep 0.5s
done

