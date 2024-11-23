# Run a test N times

if [ $# -ne 2 ]; then
   echo "Usage: sh repeat.sh TFILE N"
   exit
fi

tfile=$1
n=$2

export TEST_NGINX_LEAVE=1

for i in $(seq 1 $n); do
    dir=$(dirname $(pwd))
    prove $dir/t/$tfile;
    
    if [ $? -ne 0 ]; then
        echo "FAILED"
        exit
    else
        rm -r /tmp/nginx-test-*
    fi
done

unset $TEST_NGINX_LEAVE

