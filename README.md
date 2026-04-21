# inkctl - control panel for paper tablets

## Overview
The purpose of this project is to provide a `k9s` like interface for the reMarkable Paper Pro Move eink tablet.  In order for this to work properly, you will first need to ensure that the kernel of your device can support running `k3s`.  This is not possible on the stock kernel as it is stripped down and missing support for `netfilter` and several other things needed to support containers.  See my other project [kubernetes-on-paper](https://git.hackanooga.com/mikeconrad/kubernetes-on-paper) for detailed instructions on recompiling and flashing the kernel.  That repo also includes an all in one script that will handle all the kernel stuff for you and setup and install `k3s`.


## Prereqs
This project assumes that you have the following installed on the target system:
- k3s

## Building and Testing
The build instructions differ slightly depending on where you want to run the app.  If you are on a modern mac you can actually run the application as a desktop app using the following:

```shell
# Mac build instructions
cmake . -B build
cmake --build build

# Now run the app
./build/inkctl -platform cocoa
```

As long as you have a valid `kubeconfig` file set up you should be good to go.  To build for `reMarkable` you will first need to source the toolchain.