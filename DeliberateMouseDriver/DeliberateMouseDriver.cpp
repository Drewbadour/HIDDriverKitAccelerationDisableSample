//
//  DeliberateMouseDriver.cpp
//  DeliberateMouseDriver
//
// See the LICENSE.txt file for this sample’s licensing information.
//
// Abstract:
// A mouse driver that disables acceleration for both the mouse cursor and scroll wheel.
// Uses a standard HID match, which doesn't require any restricted entitlements.
// By overriding `handleReport`, the driver can intercept all HID packets and remove acceleration.
//

#include <os/log.h>

#include <DriverKit/DriverKit.h>
#include <HIDDriverKit/HIDDriverKit.h>

#include "DeliberateMouseDriver.h"

#include <time.h>

// To search for logs from this driver, use either: `sudo dmesg | grep DeliberateDriver` or use Console.app search to find messages that start with "DeliberateDriver".
#define Log(fmt, ...) os_log(OS_LOG_DEFAULT, "DeliberateDriver Mouse - " fmt "\n", ##__VA_ARGS__)

struct DeliberateMouseDriver_IVars
{
	/// The HID interface that the driver is handling
	IOHIDInterface* interface;
	/// The retained callback to be called when a HID report is available
	OSAction* reportAvailableAction;

	/// All of the mouse elements present for this HID interface
	OSArray* mouseElements;

	/// The current state of HID buttons for this HID interface
	uint32_t buttonState;
};

// MARK: Dext Lifecycle Management

/// Called on driver startup. Used to initialize driver memory.
bool DeliberateMouseDriver::init(void)
{
	bool result = false;

	Log("init()");

	result = super::init();
	if (result != true)
	{
		Log("init() - super::init failed.");
		goto Fail;
	}

	ivars = IONewZero(DeliberateMouseDriver_IVars, 1);
	if (ivars == nullptr)
	{
		Log("init() - Failed to allocate memory for ivars.");
		goto Fail;
	}

	// Assuming 8 buttons + 2 axes + 1 wheel as a basic mouse implementation.
	ivars->mouseElements = OSArray::withCapacity(11);
	if (ivars->mouseElements == nullptr)
	{
		Log("init() - Failed to allocate memory for mouse elements.");
		goto Fail;
	}

	Log("init() - Finished.");
	return true;

Fail:
	return false;
}

/// Called on driver startup. Used to start up driver processes.
kern_return_t DeliberateMouseDriver::Start_Impl(IOService* provider)
{
	provider->GetProvider();

	IOHIDInterface temp = {};
	kern_return_t ret = kIOReturnSuccess;
	OSArray* deviceElements = nullptr;
	bool result = false;

	Log("Start()");

	ret = Start(provider, SUPERDISPATCH);
	if (ret != kIOReturnSuccess)
	{
		Log("Start() - super::Start failed with error: 0x%08x.", ret);
		goto Exit;
	}

	// Create a callback object that allows the driver to be notified when a new packet is received from the device.
	// This function establishes your `reportAvailable` function as a callback.
	ret = CreateActionReportAvailable(sizeof(uint64_t), &(ivars->reportAvailableAction));
	if (ret != kIOReturnSuccess)
	{
		Log("Start() - Failed to create action for call to ReportAvailable with error: 0x%08x.", ret);
		goto Exit;
	}

	ivars->interface = OSDynamicCast(IOHIDInterface, provider);
	if (ivars->interface == nullptr)
	{
		Log("Start() - Failed to cast provider to IOHIDInterface.");
		ret = kIOReturnError;
		goto Exit;
	}

	// Passing the callback object when opening the interface allows the driver to receive callbacks on new packets.
	ret = ivars->interface->Open(this, 0, ivars->reportAvailableAction);
	if (ret != kIOReturnSuccess)
	{
		Log("Start() - Failed to open interface with error: 0x%08x.", ret);
		goto Exit;
	}

	// IOUserHIDEventService manages the lifecycle of the device elements, so there is no need to release them
	deviceElements = getElements();
	if (deviceElements == nullptr)
	{
		Log("Start() - Failed to get elements.");
		ret = kIOReturnInvalid;
		goto Exit;
	}

	// This populates the mouseElements array with all HID elements that refer to a mouse device.
	// It also prevents matching on other interfaces that may match our matching parameters.
	// For example, if a mouse also provides a keyboard interface, this will prevent that interface from matching to this driver.
	result = parseMouseElements(deviceElements);
	if (result == false)
	{
		Log("Start() - Matched interface contains no mouse elements. Exiting.");
		ret = kIOReturnInvalid;
		goto Exit;
	}

	ret = RegisterService();
	if (ret != kIOReturnSuccess)
	{
		Log("Start() - Failed to register service with error: 0x%08x.", ret);
		goto Exit;
	}

	Log("Start() - Finished.");
	ret = kIOReturnSuccess;
	return ret;

Exit:
	Stop(provider);
	return ret;
}

/// Called on driver cleanup. Used to stop all driver activity. Cleanup will be handled in `free`.
kern_return_t DeliberateMouseDriver::Stop_Impl(IOService* provider)
{
	kern_return_t ret = kIOReturnSuccess;
	__block _Atomic uint32_t cancelCount = 0;

	Log("Stop()");

	if (ivars != nullptr)
	{
		if (ivars->interface)
		{
			ivars->interface->Close(this, 0);
		}

		if (ivars->reportAvailableAction != nullptr)
		{
			++cancelCount;
		}
	}

	// If there's somehow nothing to cancel, "Stop" quickly and exit.
	if (cancelCount == 0)
	{
		ret = Stop(provider, SUPERDISPATCH);
		if (ret != kIOReturnSuccess)
		{
			Log("Stop() - super::Stop failed with error: 0x%08x.", ret);
		}

		Log("Stop() - Finished.");

		return ret;
	}
	// Otherwise, wait for some Cancels to get completed.

	// Retain the driver instance and the provider so the finalization can properly stop the driver
	this->retain();
	provider->retain();

	// Re-use this block, with each cancel action taking a count off, until the last cancel stops the dext
	void (^finalize)(void) = ^{

		if (__c11_atomic_fetch_sub(&cancelCount, 1U, __ATOMIC_RELAXED) <= 1) {

			kern_return_t status = Stop(provider, SUPERDISPATCH);
			if (status != kIOReturnSuccess)
			{
				Log("Stop() - super::Stop failed with error: 0x%08x.", status);
			}

			Log("Stop() - Finished.");

			this->release();
			provider->release();
		}
	};

	// All of these will call the "finalize" block, but only the final one to finish canceling will stop the dext

	if ((ivars != nullptr) && (ivars->reportAvailableAction != nullptr))
	{
		ivars->reportAvailableAction->Cancel(finalize);

		Log("Stop() - Cancels started, they will stop the dext later.");
	}
	else
	{
		provider->release();
		this->release();

		Log("Stop() - Driver stopped.");
	}

	return ret;
}

/// Called on driver cleanup. Used to clean up the `ivars`.
void DeliberateMouseDriver::free(void)
{
	Log("free()");

	if (ivars != nullptr)
	{
		OSSafeReleaseNULL(ivars->mouseElements);
	}
	IOSafeDeleteNULL(ivars, DeliberateMouseDriver_IVars, 1);

	super::free();
}

/// Called by the OS when a HID packet is received.
/// - Parameters:
///   - deviceElements: An array of HID elements that the device provides
/// - Returns: True if mouse elements are found for this device, otherwise false
bool DeliberateMouseDriver::parseMouseElements(OSArray* deviceElements)
{
	bool foundMouseElements = false;

	Log("parseMouseElements()");

	for (uint_fast32_t deviceElementIndex = 0; deviceElementIndex < deviceElements->getCount(); ++deviceElementIndex)
	{
		IOHIDElement* deviceElement = OSDynamicCast(IOHIDElement, deviceElements->getObject(deviceElementIndex));
		if (deviceElement == nullptr)
		{
			continue;
		}

		bool isMouseElement = false;
		IOHIDElementType type = deviceElement->getType();
		uint32_t usagePage = deviceElement->getUsagePage();
		uint32_t usage = deviceElement->getUsage();

		if ((type == kIOHIDElementTypeCollection) || (usage == 0))
		{
			// These are obviously not going to be mouse elements, so fast fail on them.
			continue;
		}

		// Determine whether the element contains mouse-related data.
		// Note that this implementation is very simplistic and does not support mice with additional features like horizontal scroll wheels.
		switch (usagePage)
		{
			case kHIDPage_GenericDesktop:
			{
				switch (usage)
				{
					// The driver assumes one sensor sending data on X/Y and a wheel sending info to the wheel usage.
					// A mouse with a horizontal scroll wheel might use the Z axis, which would need to be checked for additionally.
					case kHIDUsage_GD_X:
					case kHIDUsage_GD_Y:
					case kHIDUsage_GD_Wheel:
					{
						isMouseElement = true;
					} break;
				}
			} break;

			// Accept all buttons as potential mouse inputs
			case kHIDPage_Button:
			{
				isMouseElement = true;
			} break;
		}

		if (isMouseElement == true)
		{
			ivars->mouseElements->setObject(deviceElement);
			foundMouseElements = true;
		}
	}

	return foundMouseElements;
}

/// Called by the OS when a HID packet is received.
/// - Parameters:
///   - timestamp: The timestamp of the HID report
///   - report: The HID report data for this report
///   - reportLength: The length of the HID report
///   - type: The HID report type
///   - reportID: The report ID of the HID report
void DeliberateMouseDriver::handleReport(uint64_t timestamp, uint8_t* report __unused, uint32_t reportLength __unused, IOHIDReportType type __unused, uint32_t reportID)
{
	handleMouseReport(timestamp, reportID);
}

/// This is a helper function that turns button press information into a bit mask that the OS understands.
/// - Parameters:
///   - buttonState: The variable that stores the result of this function
///   - index: The button index that is being pressed or released
///   - value: The value of the button, with 0 meaning released, and other values meaning pressed
/// - Returns: The edited button state
static inline uint32_t setButtonState(uint32_t& buttonState, uint32_t index, uint32_t value)
{
	uint32_t mask = (1 << index);

	if (value == 0)
	{
		// Clear bit
		buttonState &= ~mask;
	}
	else
	{
		// Set bit
		buttonState |= mask;
	}

	return buttonState;
}

/// Handles mouse reports by passing them on to `dispatchRelativePointerEvent` and `dispatchRelativeScrollWheelEvent`.
/// Disables acceleration by passing `false` to both of these functions.
/// Since simply disabling acceleration slows down mouse and scroll inputs, values are multiplied using left shifts.
/// Consider tuning these shift values to your preference.
/// - Parameters:
///   - timestamp: The timestamp of the HID report
///   - reportID: The HID report ID for this report
void DeliberateMouseDriver::handleMouseReport(uint64_t timestamp, uint32_t reportID)
{
	IOFixed dX = 0;
	IOFixed dY = 0;
	IOFixed scrollVert = 0;

	for (uint_fast32_t mouseElementIndex = 0; mouseElementIndex < ivars->mouseElements->getCount(); ++mouseElementIndex)
	{
		IOHIDElement* element = OSDynamicCast(IOHIDElement, ivars->mouseElements->getObject(mouseElementIndex));
		if (element == nullptr)
		{
			continue;
		}

		// Don't process any events that have a different report ID than the one that is being processed.
		if (reportID != element->getReportID())
		{
			continue;
		}

		// Check for matching timestamps so elements aren't applied for the wrong timestamp, or out of order.
		uint64_t elementTimestamp = element->getTimeStamp();
		if (elementTimestamp != timestamp)
		{
			continue;
		}

		uint32_t usagePage = element->getUsagePage();
		uint32_t usage = element->getUsage();
		int32_t value = element->getValue(0);

		switch (usagePage)
		{
			case kHIDPage_GenericDesktop:
			{
				// All IOFixed values are 16.16 fixed point numbers.
				// In order to convert our input data into this format, we simply shift
				// its integer representation into the whole number portion of the representation
				// Adjust this so it is appropriate for your mouse and its data output.
				switch (usage)
				{
					case kHIDUsage_GD_X:
					{
						dX = value << 15;
					} break;
					case kHIDUsage_GD_Y:
					{
						dY = value << 15;
					} break;
					case kHIDUsage_GD_Wheel:
					{
						scrollVert = IOFixedMultiply(value << 16, -3 << 16);
					} break;
				}
			} break;
			case kHIDPage_Button:
			{
				if ((usage >= kHIDUsage_Button_1) && (usage <= kHIDUsage_Button_255))
				{
					ivars->buttonState = setButtonState(ivars->buttonState, usage - kHIDUsage_Button_1, value);
				}
			}
		}
	}

	// Passing kIOHIDPointerEventOptionsNoAcceleration/kIOHIDScrollEventOptionsNoAcceleration
	// are the same as passing false to the acceleration parameter of these methods.
	// These arguments are included for completeness only, and are not required to disable acceleration if you pass false.
	// However, passing one of the NoAcceleration flags will override the boolean state of acceleration.
	dispatchRelativePointerEvent(timestamp, dX, dY, ivars->buttonState, kIOHIDPointerEventOptionsNoAcceleration, false);
	dispatchRelativeScrollWheelEvent(timestamp, scrollVert, 0, 0, kIOHIDScrollEventOptionsNoAcceleration, false);
}
