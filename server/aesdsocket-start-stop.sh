case "$1" in
    start)
        echo "Starting aesdsocket"
        #start command
        start-stop-daemon --start --oknodo --make-pidfile --pidfile /var/run/aesdsocket.pid --chdir /home/jarrett/projects/assignments-3-and-later-JarrettSiler/server --exec ./aesdsocket -- -d
        ;;
    stop)
        echo "Stopping aesdsocket"
        start-stop-daemon --stop --signal SIGTERM --pidfile /var/run/aesdsocket.pid
        ;;
    *)
        echo "Usage: $0 {start|stop}"
    exit 1
esac

exit 0