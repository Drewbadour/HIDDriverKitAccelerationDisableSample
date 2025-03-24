# Matching a HID Device with HIDDriverKit

Disable mouse and scroll wheel acceleration with HIDDriverKit.

[link_news_DemystifyCodeSigningForDriverKit]:https://developer.apple.com/news/?id=c63qcok4
[link_article_DisablingEnablingSystemIntegrityProtection]:https://developer.apple.com/documentation/security/disabling_and_enabling_system_integrity_protection
[link_article_TestingSystemExtensions]:https://developer.apple.com/documentation/driverkit/debugging_and_testing_system_extensions?language=objc
[link_framework_SystemExtensions]:https://developer.apple.com/documentation/systemextensions
[link_article_InstallingSystemExtensionsAndDrivers]:https://developer.apple.com/documentation/systemextensions/installing_system_extensions_and_drivers
[link_news_MatchYourDriverKitDrivers]:https://developer.apple.com/news/?id=zk5xdwbn
[link_framework_IOHIDEventService]:https://developer.apple.com/documentation/hiddriverkit/iohideventservice
[link_framework_IOUserHIDEventService]:https://developer.apple.com/documentation/hiddriverkit/iouserhideventservice#Specify-the-Drivers-Personality-Information

## Overview

This sample code project shows how a DriverKit extension (dext) can be used to match to a HID interface, as well as how to edit the packets that the HID interface sends to the host device.

Please note that this sample is not configured to work with iPadOS.

The sample project contains two targets:
* `DeliberateDriverLoader` - A SwiftUI app for macOS. Use this app to install or update the driver.
* `DeliberateMouseDriver` - The dext itself, which contains drivers that should work for any HID mouse.

## Configure the Sample Code Project

Building this sample will require an Apple developer team. Please make sure to select your team in the `DeliberateDriverLoader` and `DeliberateMouseDriver` targets.

To run the sample code project, you first need to build and run `DeliberateDriverLoader`, which installs the driver.

If you need additional help with DriverKit codesigning, consider reading [Demystify code signing for DriverKit][link_news_DemystifyCodeSigningForDriverKit].

## Install and run the driver extension on macOS

Before running the app, consider enabling System Extension developer mode to simplify your development experience. This can be achieved by the following:

1. Temporarily turn off SIP, as described in the article [Disabling and Enabling System Integrity Protection][link_article_DisablingEnablingSystemIntegrityProtection].
1. Confirm that SIP is disabled with the Terminal command `csrutil status`.
1. Enter dext development mode with `systemextensionsctl developer`, as described in the article [Debugging and Testing System Extensions][link_article_TestingSystemExtensions].

To run the sample app in macOS, use the scheme selector to select the `DeliberateDriverLoader` scheme and the "My Mac" destination. If you have not enabled System Extension developer mode, then you will need to copy the app to the Applications folder and launch the app after building. However, if System Extension developer mode is enabled, you can run the app directly from Xcode.

The `DeliberateDriverLoader` target declares the `DeliberateMouseDriver` as a dependency, so building the app target builds the dext and its installer together. When run, the `DeliberateDriverLoader` shows a single window with a text label that says "Deliberate Driver Loader". Below this, it shows an "Install/Update Driver" button, and a "Uninstall Driver" button. Click "Install/Update Driver" to perform the installation, or upgrade the driver to the latest version.

To install the dext on macOS, the app uses the [System Extensions][link_framework_SystemExtensions] framework to install and activate the dext, as described in [Installing System Extensions and Drivers][link_article_InstallingSystemExtensionsAndDrivers].

- Note: This call may prompt a "System Extension Blocked" dialog, which explains that `SimpleDriverLoader` tried to install a new system extension. To complete the installation, open System Preferences and go to the Security & Privacy pane. Unlock the pane if necessary, and click "Allow" to complete the installation. To confirm installation of the `DeliberateDriverLoader` extension, run `systemextensionsctl list` in Terminal.

## How this driver works

When reporting HID packets to the operating system via HIDDriverKit, use the `dispatch...` functions defined by [IOHIDEventService][link_framework_IOHIDEventService]. This example uses `dispatchRelativePointerEvent` and `dispatchRelativeScrollWheelEvent`. Both of these functions offer an `accelerate` parameter, as well as options to disable scroll acceleration that you can call in `kIOHIDPointerEventOptionsNoAcceleration` and `kIOHIDScrollEventOptionsNoAcceleration`. However, when this is done with no change to the `dX`, `dY`, etc. values, then mouse inputs will be significantly less sensitive than usual. So the driver also multiplies the passed values to return them to higher sensitivity that a user might expect from accelerated inputs.

## Matching a HID Interface

Matching on HID devices requires no restricted entitlements. Matching uses HID matching keys like `VendorID`, `ProductID`, `PrimaryUsagePage`, and `PrimaryUsage`. All of the matching dictionaries for this driver use `VendorID` and `ProductID`.

### `Info.plist`

Consider this example from the source code:

```xml
<key>Logitech Unifying Receiver</key>
<dict>
    <key>CFBundleIdentifier</key>
    <string>$(PRODUCT_BUNDLE_IDENTIFIER)</string>
    <key>CFBundleIdentifierKernel</key>
    <string>com.apple.kpi.iokit</string>
    <key>IOClass</key>
    <string>AppleUserHIDEventService</string>
    <key>IOProviderClass</key>
    <string>IOHIDInterface</string>
    <key>IOResourceMatch</key>
    <string>IOKit</string>
    <key>IOUserClass</key>
    <string>DeliberateMouseDriver</string>
    <key>IOUserServerName</key>
    <string>$(PRODUCT_BUNDLE_IDENTIFIER)</string>
    <key>ProductID</key>
    <integer>50475</integer>
    <key>VendorID</key>
    <integer>1133</integer>
</dict>
```

The key elements are `IOProviderClass` set to `IOHIDInterface`, which means that this entry matches a HID interface. Since the driver class extends `IOUserHIDEventService`, make sure to update your `IOClass` to [the value its documentation suggests][link_framework_IOUserHIDEventService]. This means setting the `IOClass` to `AppleUserHIDEventService`.

For more information on matching drivers, check out the [Match your DriverKit drivers with the right USB device][link_news_MatchYourDriverKitDrivers] article and the articles it links.
