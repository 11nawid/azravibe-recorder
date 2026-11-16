// macOS target enumeration implementation.
// Screens come from CGGetActiveDisplayList; windows from CGWindowListCopyWindowInfo
// filtered to on-screen, layer == 0, large-enough windows owned by other processes.
#include "Capture.h"

#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>

#include <algorithm>

QVector<TargetItem> listTargetsMac(uint64_t ownWindow) {
    (void)ownWindow;   // own-process windows are excluded by pid instead

    QVector<TargetItem> out;

    // ---- screens ----
    uint32_t count = 0;
    CGDirectDisplayID displays[16] = {0};
    if (CGGetActiveDisplayList(16, displays, &count) == kCGErrorSuccess && count > 0) {
        // "Entire display" = the union of all monitors' bounds.
        CGRect unionBounds = CGDisplayBounds(displays[0]);
        for (uint32_t i = 1; i < count; ++i)
            unionBounds = CGRectUnion(unionBounds, CGDisplayBounds(displays[i]));

        TargetItem whole;
        whole.type = TargetItem::Type::Screen;
        whole.screenRect = QRect((int)unionBounds.origin.x, (int)unionBounds.origin.y,
                                 (int)unionBounds.size.width, (int)unionBounds.size.height);
        whole.label = QStringLiteral("Screen — Entire display (%1×%2)")
                          .arg((int)unionBounds.size.width).arg((int)unionBounds.size.height);
        out << whole;

        for (uint32_t i = 0; i < count; ++i) {
            const CGRect b = CGDisplayBounds(displays[i]);
            QRect r((int)b.origin.x, (int)b.origin.y, (int)b.size.width, (int)b.size.height);
            if (r.width() < 2 || r.height() < 2) continue;

            TargetItem item;
            item.type = TargetItem::Type::Screen;
            item.screenRect = r;
            item.label = QStringLiteral("Screen — Monitor %1 (%2×%3)%4")
                             .arg(i + 1).arg(r.width()).arg(r.height())
                             .arg(displays[i] == CGMainDisplayID() ? QStringLiteral(" · primary") : QString());
            out << item;
        }
    }

    // ---- windows ----
    CGWindowListOption opts = kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements;
    CFArrayRef windows = CGWindowListCopyWindowInfo(opts, kCGNullWindowID);
    if (windows) {
        const pid_t ownPid = getpid();
        const CFIndex n = CFArrayGetCount(windows);

        for (CFIndex i = 0; i < n; ++i) {
            NSDictionary* w = (__bridge NSDictionary*)CFArrayGetValueAtIndex(windows, i);

            NSNumber* layer  = w[(__bridge NSString*)kCGWindowLayer];
            if (layer && layer.intValue != 0) continue;            // only normal app windows

            NSNumber* alphaN = w[(__bridge NSString*)kCGWindowAlpha];
            if (alphaN && alphaN.floatValue <= 0.05f) continue;    // hidden / fully transparent

            NSDictionary* bounds = w[(__bridge NSString*)kCGWindowBounds];
            if (!bounds) continue;
            const double ww = [bounds[@"Width"] doubleValue];
            const double hh = [bounds[@"Height"] doubleValue];
            if (ww < 64 || hh < 64) continue;

            NSNumber* ownerPidN = w[(__bridge NSString*)kCGWindowOwnerPID];
            if (ownerPidN && ownerPidN.intValue == ownPid) continue;   // never offer ourselves

            NSString* owner = w[(__bridge NSString*)kCGWindowOwnerName];
            NSString* title = w[(__bridge NSString*)kCGWindowName];
            if (!owner && !title) continue;

            NSString* text = (title.length > 0)
                ? [NSString stringWithFormat:@"%@ — %@", owner ? owner : @"", title]
                : (owner ? owner : @"Window");
            if (text.length == 0) continue;

            NSNumber* windowNumberN = w[(__bridge NSString*)kCGWindowNumber];
            if (!windowNumberN) continue;

            TargetItem item;
            item.type = TargetItem::Type::Window;
            item.windowId = (uint64_t)windowNumberN.unsignedLongValue;
            item.label = QStringLiteral("Window — %1").arg(QString::fromNSString(text).trimmed());
            out << item;
        }
        CFRelease(windows);
    }

    return out;
}
