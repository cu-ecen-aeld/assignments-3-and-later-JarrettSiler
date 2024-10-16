case "$1" in
    start)
        #start command
        start-stop-daemon -S -n aesdsocket -a /usr/bin/
        echo "Starting aesdsocket"
        ;;
    stop)
        start-stop-daemon -K -n aesdsocket
        echo "Stopping aesdsocket"
        ;;
    *)
        echo "Usage: $0 {start|stop}"
    exit 1
esac

exit 0