/*
 * Poor man's tabs (window_tabs.h), macOS: since macOS 14 an app comes to
 * the front only when the active app hands over activation. The instance
 * whose tab strip was clicked yields to the clicked tab's process, which
 * then activates itself.
 */

#import <AppKit/AppKit.h>

#include "window_tabs.h"

void window_tabs_platform_yield(int pid)
{
	@autoreleasepool {
		NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
		if (!app)
			return;
		if (@available(macOS 14.0, *))
			[NSApp yieldActivationToApplication:app];
		else
			[app activateWithOptions:0];
	}
}

void window_tabs_platform_activate(void)
{
	@autoreleasepool {
		if (@available(macOS 14.0, *))
			[NSApp activate];
		else
			[NSApp activateIgnoringOtherApps:YES];
	}
}

int window_tabs_platform_frontmost_pid(void)
{
	@autoreleasepool {
		NSRunningApplication *app = [[NSWorkspace sharedWorkspace] frontmostApplication];
		return app ? (int)app.processIdentifier : -1;
	}
}
