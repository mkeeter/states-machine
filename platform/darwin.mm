extern "C" {
#include "app.h"
#include "instance.h"
#include "window.h"
}
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <Foundation/NSObjCRuntime.h>
#import <objc/runtime.h>

@interface InstanceHandle : NSObject {
@public
    instance_t* instance;
}
@end
@implementation InstanceHandle
@end

@interface Glue : NSObject {
@public
    app_t* app;
}
-(void) onClose;
-(void) onAboutMenu;
@end

@implementation Glue

-(void) onClose {
    NSWindow* window = [[NSApplication sharedApplication] keyWindow];
    if (window) {
        InstanceHandle* handle = objc_getAssociatedObject(
            window, "WINDOW_INSTANCE");
        if (handle) {
            glfwSetWindowShouldClose(handle->instance->window, 1);
        } else {
            [window close];
        }
    }
}

-(void)onAboutMenu {
    extern const char* GIT_REV;
    NSString *version = [NSString stringWithFormat:@"Version: %s", GIT_REV];
    NSDictionary* d = @{
        NSAboutPanelOptionApplicationVersion: version,
    };
    [[NSApplication sharedApplication] orderFrontStandardAboutPanelWithOptions:d];
}

@end

static Glue* GLUE = NULL;

extern "C" void platform_window_bind(GLFWwindow* w) {
    InstanceHandle* handle = [[InstanceHandle alloc] init];
    handle->instance = (instance_t*)glfwGetWindowUserPointer(w);

    objc_setAssociatedObject(glfwGetCocoaWindow(w), "WINDOW_INSTANCE",
                             handle, OBJC_ASSOCIATION_RETAIN);
}

extern "C" void platform_init(app_t* app, int argc, char** argv)
{
    GLUE = [[Glue alloc] init];
    GLUE->app = app;

    app_start(app);

    // Build a file menu
    NSMenu *fileMenu = [[[NSMenu alloc] initWithTitle:@"File"] autorelease];
    NSMenuItem *fileMenuItem = [[[NSMenuItem alloc]
        initWithTitle:@"File" action:NULL keyEquivalent:@""] autorelease];
    [fileMenuItem setSubmenu:fileMenu];

    NSMenuItem *close = [[[NSMenuItem alloc]
        initWithTitle:@"Close"
        action:@selector(onClose) keyEquivalent:@"w"] autorelease];
    close.target = GLUE;
    [fileMenu addItem:close];


    NSApplication * nsApp = [NSApplication sharedApplication];
    [nsApp.mainMenu insertItem:fileMenuItem atIndex:1];

    // Patch the "About" menu item to call our custom function
    NSMenu* appMenu = [nsApp.mainMenu itemWithTitle:@""].submenu;
    NSMenuItem* aboutItem = [appMenu itemAtIndex:0];
    aboutItem.action = @selector(onAboutMenu);
    aboutItem.target = GLUE;
}

extern "C" void platform_warning(const char* title, const char* text) {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:[NSString stringWithUTF8String:title]];
    [alert setInformativeText:[NSString stringWithUTF8String:text]];
    [alert addButtonWithTitle:@"Okay"];
    [alert runModal];
}
