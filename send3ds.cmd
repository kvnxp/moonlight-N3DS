# allow overriding the target host via first argument or environment variable
HOST="${1:-${HOST:-192.168.1.34}}"

if command -v 3dslink >/dev/null 2>&1; then
    # verify that ping actually works and report status for debugging
    if ! command -v ping >/dev/null 2>&1; then
        echo "[warning] ping command not found; skipping connectivity check"
        PING_OK=0
    else
        echo "===> testing network reachability to $HOST"
        PING_OK=1
        # try unix-style ping first, with short timeout option (-W 1 second)
        if OUT=$(ping -c 1 -W 1 "$HOST" 2>&1); then
            PING_OK=0
        else
            # fall back to Windows syntax; use -w for 1000ms timeout and capture output
            if OUT=$(ping -n 1 -w 1000 "$HOST" 2>&1); then
                # Windows ping exits 0 even if unreachable; look for TTL indicating a valid response
                if echo "$OUT" | grep -iq 'TTL='; then
                    PING_OK=0
                else
                    echo "[debug] ping output indicates failure:"
                    echo "$OUT" >&2
                fi
            else
                # ping command itself failed, keep PING_OK=1
                echo "[debug] ping command failed to execute: $OUT" >&2
            fi
        fi
    fi

    if [ "$PING_OK" -eq 0 ]; then
        echo "===> uploading moonlight.3dsx via 3dslink (-a)"
        3dslink -a "$HOST" moonlight.3dsx || echo "[warning] 3dslink returned non-zero"
    else
        echo "[error] device not connected: unable to reach $HOST (ping exit $PING_OK)"
    fi
else
    echo "===> 3dslink not found; skipping automatic upload"
fi