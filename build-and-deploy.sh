#!/bin/env bash
source /opt/codex/chiappa/5.6.75/environment-setup-cortexa55-remarkable-linux
cmake . -B build
cmake --build build
scp build/k8s_dashboard root@10.200.34.122:/
