

while true;
do
        server=`ps -aux | grep "\bsyncd\b" | grep -v grep`
        if [ ! "$server" ]; then
            echo "syncd exit"
            /dist/syncd.py -c /dist/paysvr_conf.json  &
        fi
        sleep 1
done

