#!/bin/env bash
source /opt/codex/chiappa/5.6.75/environment-setup-cortexa55-remarkable-linux
cmake . -B build
cmake --build build
scp build/inkctl root@10.200.34.122:/home/root/
scp scripts/inkctl.service root@10.200.34.122:/etc/systemd/system/
scp scripts/inkctl-display.service root@10.200.34.122:/etc/systemd/system/
ssh root@10.200.34.122 "systemctl daemon-reload && systemctl enable inkctl.service"