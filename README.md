# mdeGranular

This is mdeGranular~, a real-time, multi-channel, multi-voice,
multi-transposition granular synthesis external for Max/MSP and PD.

Documentation is on the [wiki](./wiki).

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

