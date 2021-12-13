# mdeGranular

This is mdeGranular~, a real-time, multi-channel, multi-voice,
multi-transposition granular synthesis external for Max/MSP and PD.

Documentation is on the [wiki](../../wiki).

## Compilation

To compile for MaxMSP on OSX you'll need XCode and the most recent MaxSDK.
There's a sample XCode project in the max folder. This was created for Max SDK
8.0.3 using XCode 11.3.1. It will not work directly as is, rather, you need to
put the xcode folder (containing mdegranular~.xcodeproj) into the MaxSDK folder
"source/audio" alongside the MaxMSP example projects. From there it should build
against the MaxMSP libraries. You can get the Max SDK from
https://cycling74.com/sdk/max-sdk-8.0.3/html/index.html

I haven't compiled for Max or PD on windows or Linux but the MaxSDK should be
able to create a windows external using Visual Studio. For PD on Linux or
Windows I assume that the included pd-lib-builder project should take care of
compilation.


Michael Edwards, March 9th 2020
m@michael-edwards.org
https://www.michael-edwards.org

## Install

1. Clone project with `git clone --recurse-submodules` into your Max folder
2. Change `DSTROOT` variable in `maxmspsdk.xcconfig` to set build destination

## Fork Additions

- Added `max-sdk` git submodule. You can change the version of max by [checking out another tagged release](https://stackoverflow.com/a/1778247/8876321)
    ```sh
    cd max-sdk
    git checkout v7.0.3 # or v7.1.0 v7.3.3 v8.0.3
    cd ..
    git add max-sdk
    git commit -m "change max version v7.1.0"
    git push
    ```
- added macOS 10.11 sdk for pre v8.0.0 Max builds
- add local `maxmspsdk.xcconfig` to improve portability

### maxmspsdk.xcconfig

The `maxmspsdk.xcconfig` in the `xcode` sets a couple of global paths in the xcode project. Some of these variables you can change others you should leave alone. The ones to change are

- `PRODUCT_VERSION`: which version of the max sdk are you using?
- `DSTROOT`: destination of the built external (project relative directory)

### Current Issues

- [x] 'QuickTime/QuickTime.h' file not found

    Building Max 7 externals on macOS 10.12 and later may result in the above error. QuickTime frame work has been deprecated so a macOS 10.11 sdk has been included in the repo for ease.

    See the following for details

    - [Solved: macOS Sierra, Xcode 8, missing quicktime.h](https://sdk.steinberg.net/viewtopic.php?t=229)
    - [macOS sdks](https://github.com/phracker/MacOSX-SDKs)
    - paste `/PATH/TO/MacOSX10.11.sdk` as Base SDK. will say sdk not found but still works
    - Or paste "MacOSX10.11.sdk" folder in `/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs`  and select Base SDK
