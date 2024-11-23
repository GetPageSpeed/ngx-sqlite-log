# Watch error.log and quit Nginx when an alert appears

while [ 1 ]; do
   cat /tmp/error.log | grep "alert"
   if [ $? -eq 0 ]; then
      echo "Found [alert] in error.log"
      sudo /usr/local/nginx/sbin/nginx -s quit
      break
   fi
   sleep 0.3s
done

