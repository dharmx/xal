#include <X11/XKBlib.h>
#include <X11/extensions/XInput2.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

const u_int UTF_STR_LEN = 10;

const u_int EXIT_DPY_NOT_OPEN = 1;
const u_int EXIT_EXT_NOT_FOUND = 2;
const u_int EXIT_XI_QRY_RET_BAD = 3;
const u_int EXIT_XI_QRY_RET_FAIL = 4;

const char* MOD_KEY = "Alt_L";
const char* DISPLAY_NAME = ":0";
const Bool END_NEWLINE = True;

int
main(int argc, char** argv) {
    Display* display         = XOpenDisplay(DISPLAY_NAME);
    if (display == NULL) {
        fprintf(stderr, "Could not open display: %s\n", DISPLAY_NAME);
        exit(EXIT_DPY_NOT_OPEN);
    }

    int xi_opcode;
    int xi_events;
    int xi_errors;
    if (!XQueryExtension(display, "XInputExtension", &xi_opcode, &xi_events, &xi_errors)) {
        fprintf(stderr, "Could not find XInputExtension\n");
        exit(EXIT_EXT_NOT_FOUND);
    }

    int xi_major_version = 2;
    int xi_minor_version = 0;
    int query_return     = XIQueryVersion(display, &xi_major_version, &xi_minor_version);
    if (query_return == BadRequest) {
        fprintf(stderr, "Query for XInputExtension returned BadRequest\n");
        exit(EXIT_XI_QRY_RET_BAD);
    } else if (query_return != Success) {
        fprintf(stderr, "Need XInputExtension 2.0 got %d.%d\n", xi_major_version, xi_minor_version);
        exit(EXIT_XI_QRY_RET_FAIL);
    }

    int xkb_opcode;
    int xkb_events;
    int xkb_errors;
    int xkb_major_version;
    int xkb_minor_version;
    if (!XkbQueryExtension(display, &xkb_opcode, &xkb_events, &xkb_errors, &xkb_major_version, &xkb_minor_version)) {
        fprintf(stderr, "Could not find XKBlib\n");
        exit(EXIT_EXT_NOT_FOUND);
    }

    Window window = DefaultRootWindow(display);
    XIEventMask mask;
    memset(&mask, None, sizeof(XIEventMask));
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_LASTEVENT);
    mask.mask     = calloc(mask.mask_len, sizeof(char));

    XISetMask(mask.mask, XI_RawKeyPress);
    XISetMask(mask.mask, XI_RawKeyRelease);
    XISelectEvents(display, window, &mask, 1);
    XSync(display, False);
    XFree(mask.mask);

    XkbSelectEventDetails(display, XkbUseCoreKbd, XkbStateNotify, XkbGroupStateMask, XkbGroupStateMask);
    XkbStateRec state;
    XkbGetState(display, XkbUseCoreKbd, &state);
    int group = state.group;

    XEvent event;
    memset(&event, None, sizeof(XEvent));
    XGenericEventCookie* cookie;

    Bool modkey_pressed  = False;
    Bool modkey_released = False;
    u_int count          = 0;
    u_char symbols[UTF_STR_LEN];
    memset(&symbols, None, sizeof(u_char) * UTF_STR_LEN);

    while (!modkey_released) {
        cookie = (XGenericEventCookie*) &event.xcookie;
        XNextEvent(display, &event);

        if (XGetEventData(display, cookie)) {
            if (cookie->type == GenericEvent && cookie->extension == xi_opcode) {
                if (cookie->evtype == XI_RawKeyRelease || cookie->evtype == XI_RawKeyPress) {
                    XIRawEvent* raw_event = cookie->data;
                    KeySym keysym         = XkbKeycodeToKeysym(display, raw_event->detail, group, 0);
                    KeyCode keycode       = XKeysymToKeycode(display, keysym);
                    if (NoSymbol == keysym) {
                        if (group == 0) continue;
                        else {
                            keysym = XkbKeycodeToKeysym(display, raw_event->detail, 0, 0);
                            if (NoSymbol == keysym) continue;
                        }
                    }

                    char* keyname = XKeysymToString(keysym);
                    if (keyname == NULL) continue;
                    if (keycode == 37 || keycode == 50 || keycode == 51 || keycode == 36) continue;

                    Bool modkey_match = strcmp(MOD_KEY, keyname) == 0;
                    if (keycode >= 10 && keycode <= 19 || keycode >= 24 && keycode <= 58 || modkey_match) {
                        switch (cookie->evtype) {
                            case XI_RawKeyPress:
                                if (modkey_match) modkey_pressed = True;
                                if (!modkey_pressed) continue;
                                if (count == UTF_STR_LEN - 1) continue;

                                if (modkey_pressed && !modkey_match) {
                                    symbols[count] = keyname[0];
                                    ++count;
                                }
                                break;
                            case XI_RawKeyRelease:
                                if (modkey_match) modkey_released = True;
                                break;
                            default: continue;
                        }
                    }
                }
            }
            XFreeEventData(display, cookie);
        } else {
            if (event.type == xkb_events) {
                XkbEvent* xkb_event = (XkbEvent*) &event;
                if (xkb_event->any.xkb_type == XkbStateNotify) {
                    group = xkb_event->state.group;
                }
            }
        }
    }

    symbols[count] = '\0';
    printf("\\u%s", symbols);
    if (END_NEWLINE) printf("\n");
    fflush(stdout);
    XFlush(display);
    XCloseDisplay(display);
    return EXIT_SUCCESS;
}
