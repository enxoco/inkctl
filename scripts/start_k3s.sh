#!/bin/env bash

# 1. Load networking modules (Required every boot)
modprobe nf_tables nf_tables_ipv4 nft_compat || exit 1

# 2. Ensure PATH includes our custom sbin
export PATH="$PATH:/home/root/sbin"

# 3. Execution variables
K3S_EXEC="/home/root/sbin/k3s"
LOG_FILE="/home/root/k3s.log"
DATA_DIR="/home/root/k3s_data"
# 4. Check if already running to prevent port conflicts
if pgrep -f "k3s server" > /dev/null; then
    echo "K3s is already running."
    exit 0
fi

echo "Starting K3s Server..."

# 5. Start K3s with your optimized Paper Pro flags
# Using nohup and setsid ensures it survives even if the dashboard blips
nohup setsid $K3S_EXEC server \
    --data-dir "$DATA_DIR" \
    --disable local-storage \
    --disable metrics-server \
    --disable-cloud-controller \
    --kube-apiserver-arg=max-requests-inflight=10 \
    --kube-apiserver-arg=max-mutating-requests-inflight=5 \
    --kubelet-arg=enforce-node-allocatable=pods \
    --snapshotter=native \
    --flannel-backend=host-gw > "$LOG_FILE" 2>&1 < /dev/null &

# 6. Minimal wait for the PID to appear
sleep 2
if pgrep -f "k3s server" > /dev/null; then
    echo "K3s process initialized."
else
    echo "Failed to start K3s. Check $LOG_FILE"
    exit 1
fi