./Server & > /dev/null
S_PID=$!

# wait for server startup
sleep 1

./$1
T_PID=$!

wait $S_PID
S_CODE=$?
wait $T_PID
T_CODE=$?

if [[ $S_CODE -ne 0 ]]; then
    echo "Server exit code is $S_CODE"
    return $S_CODE
fi
if [[ $T_CODE -ne 0 ]]; then
    echo "Test exit code is $T_CODE"
    return $T_CODE
fi

echo "Test and server have exited succesfully"
