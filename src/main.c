// Copyright (c) 2023 licktheroom //

/*
    I can't remember where I found to do this, so you'll have to do your own research.
    
    This does not load OpenGL functions, 
    see https://www.khronos.org/opengl/wiki/Load_OpenGL_Functions if you wanna do that.
*/

// DEFINES //

#define WN_NAME "xcb-opengl"

// HEADERS //

// STANDARD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// X11

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>

#include <xcb/xcb.h>

// OPENGL

#include <GL/gl.h>
#include <GL/glx.h>

// STATIC VARIABLES //

static struct 
{
    struct {
        Display *display;
    } xlib;

    struct
    {
        xcb_connection_t *connection;
        xcb_window_t window;

        xcb_atom_t close_event;
    } xcb;

    struct {
        GLXContext context;
        GLXDrawable drawable;
        GLXWindow window;
    } gl;

    bool should_close;

    struct
    {
        int width, height;
    } window;
} game;

// FUNCTIONS //

void
clean_up(void);

void
window_error_print(xcb_generic_error_t *error);

bool 
init(void);

void
input(void);

void
render(void);

bool
window_create(void);

bool
window_get_close_event(void);

// MAIN //

int
main()
{
    // Set basic window data
    game.window.width = game.window.height = 300;
    game.should_close = false;

    // Init
    if(!init()) {
        clean_up();
        return -1;
    }
    
    // Wait until we should close
    while(game.should_close == false)
    {
        input();
        render();
    }
    
    clean_up();
    return 0;
}

// FUNCTIONS //

void
clean_up(void)
{
    fprintf(stdout, "Exiting.\n");

    glXDestroyWindow(game.xlib.display, game.gl.window);
    xcb_destroy_window(game.xcb.connection, game.xcb.window);
    glXDestroyContext(game.xlib.display, game.gl.context);
    XCloseDisplay(game.xlib.display);
}

// As far as I could find, there are very few recources on XCB error codes
// All I can say is look through xproto.h or pray google finds it
void
window_error_print(xcb_generic_error_t *error)
{
    if(error == NULL)
        return;

    const char *names[] = {
        "XCB_REQUEST",
        "XCB_VALUE",
        "XCB_WINDOW",
        "XCB_PIXMAP",
        "XCB_ATOM",
        "XCB_CURSOR",
        "XCB_FONT",
        "XCB_MATCH",
        "XCB_DRAWABLE",
        "XCB_ACCESS",
        "XCB_ALLOC",
        "XCB_COLORMAP",
        "XCB_G_CONTEXT",
        "XCB_ID_CHOICE",
        "XCB_NAME",
        "XCB_LENGTH",
        "XCB_IMPLEMENTATION"
    };

    switch(error->error_code)
    {
        case XCB_REQUEST:
        case XCB_MATCH:
        case XCB_ACCESS:
        case XCB_ALLOC:
        case XCB_NAME:
        case XCB_LENGTH:
        case XCB_IMPLEMENTATION:
            {
                xcb_request_error_t *er = (xcb_request_error_t *)error;

                fprintf(stderr, "REQUEST ERROR\n"
                                "%s\n"
                                "error_code: %d\n"
                                "major: %d\n"
                                "minor: %d\n",
                                names[er->error_code],
                                er->error_code,
                                er->major_opcode,
                                er->minor_opcode);
            }
            break;

        case XCB_VALUE:
        case XCB_WINDOW:
        case XCB_PIXMAP:
        case XCB_ATOM:
        case XCB_CURSOR:
        case XCB_FONT:
        case XCB_DRAWABLE:
        case XCB_COLORMAP:
        case XCB_G_CONTEXT:
        case XCB_ID_CHOICE:
            {
                xcb_value_error_t *er = (xcb_value_error_t *)error;

                fprintf(stderr, "VALUE ERROR\n"
                                "%s\n"
                                "error_code: %d\n"
                                "major: %d\n"
                                "minor: %d\n",
                                names[er->error_code],
                                er->error_code,
                                er->major_opcode,
                                er->minor_opcode);
            }
            break;
    }
}

bool
init(void)
{
    fprintf(stdout, "Loading.\n");

    if(!window_create() ||
       !window_get_close_event()
    ) {
        fprintf(stderr, "\nInitialization failed!\n");
        return false;
    }

    glViewport(0, 0, game.window.width, game.window.height);

    fprintf(stdout, "Initialization finished.\n");

    return true;
}

// See https://xcb.freedesktop.org/tutorial/events/
void
input(void)
{
    while(true)
    {
        xcb_generic_event_t *event = xcb_poll_for_event(game.xcb.connection);

        if(event == NULL)
            break;

        switch(event->response_type & ~0x80)
        {
            // if it's a message about our window 
            case XCB_CLIENT_MESSAGE:
            {
                unsigned int ev = 
                        (*(xcb_client_message_event_t *)event).data.data32[0];

                // if it's the close event
                if(ev == game.xcb.close_event)
                    game.should_close = true;
            }

            break;

            case XCB_CONFIGURE_NOTIFY:
            {
                int new_width = (*(xcb_configure_notify_event_t *)event).width;
                int new_height = 
                            (*(xcb_configure_notify_event_t *)event).height;

                if(new_height != game.window.height || 
                   new_width != game.window.width) {
                    game.window.width = new_width;
                    game.window.height = new_height;

                    glViewport(0, 0, game.window.width, game.window.height);
                   }
            }

            break;
        }

        free(event); // Always free your event!
    }
}

void
render()
{
    // Clear the buffer
    glClearColor(0.0, 1.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    // We'd do our drawing here 

    // Swap buffers
    glXSwapBuffers(game.xlib.display, game.gl.drawable);
}

// See https://xcb.freedesktop.org/tutorial/basicwindowsanddrawing/
// and https://xcb.freedesktop.org/tutorial/events/
// and https://xcb.freedesktop.org/windowcontextandmanipulation/
bool
window_create(void)
{
    // Create the connection
    game.xlib.display = XOpenDisplay(0);
    if(!game.xlib.display) {
        fprintf(stderr, "Failed to open X display!\n");

        return false;
    }

    game.xcb.connection = XGetXCBConnection(game.xlib.display);

    if(!game.xcb.connection) {
        fprintf(stderr, "Failed to create connection to Xorg!\n");

        return false;
    }

    XSetEventQueueOwner(game.xlib.display, XCBOwnsEventQueue);

    // Get screen
    int def_screen = DefaultScreen(game.xlib.display);

    const xcb_setup_t *setup = xcb_get_setup(game.xcb.connection);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    for(int screen_num = def_screen;
        iter.rem && screen_num > 0;
        screen_num--, xcb_screen_next(&iter));

    xcb_screen_t *screen = iter.data;

    // Find GL FB
    int vis_id = 0;
    int num_fb_configs = 0;

    const int vis_attribs[] = {
		GLX_X_RENDERABLE, True,
		GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
		GLX_RENDER_TYPE, GLX_RGBA_BIT,
		GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
		GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8,
		GLX_ALPHA_SIZE, 8,
		GLX_DEPTH_SIZE, 24,
		GLX_STENCIL_SIZE, 8,
		GLX_DOUBLEBUFFER, True,
	};

    const GLXFBConfig *fb_configs = glXChooseFBConfig(game.xlib.display,
                                                      def_screen,
                                                      vis_attribs,
                                                      &num_fb_configs);

    if(!fb_configs || num_fb_configs == 0) {
        fprintf(stderr, "Failed to find an OpenGL FB!\n");

        return false;
    }

    GLXFBConfig fb_config = fb_configs[0];
    glXGetFBConfigAttrib(game.xlib.display, fb_config, GLX_VISUAL_ID, &vis_id);

    // Create GLX context

    game.gl.context = glXCreateNewContext(game.xlib.display,
                                           fb_config,
                                           GLX_RGBA_TYPE,
                                           0,
                                           True);

    if(!game.gl.context) {
        fprintf(stderr, "Failed to create an OpenGL context!\n");

        return false;
    }

    // Error data, used later
    xcb_void_cookie_t cookie;
    xcb_generic_error_t *error;

    // Create colormap
    xcb_colormap_t colormap = xcb_generate_id(game.xcb.connection);

    cookie = xcb_create_colormap_checked(game.xcb.connection,
                                         XCB_COLORMAP_ALLOC_NONE,
                                         colormap,
                                         screen->root,
                                         vis_id);

    error = xcb_request_check(game.xcb.connection, cookie);

    if(error != NULL) {
        fprintf(stderr, "Failed to create XCB colormap!\n");

        window_error_print(error);

        free(error);
        return false;
    }

    // Create window
    game.xcb.window = xcb_generate_id(game.xcb.connection);

    const int eventmask = XCB_EVENT_MASK_EXPOSURE | 
                          XCB_EVENT_MASK_KEY_PRESS | 
                          XCB_EVENT_MASK_KEY_RELEASE | 
                          XCB_EVENT_MASK_BUTTON_PRESS | 
                          XCB_EVENT_MASK_BUTTON_RELEASE | 
                          XCB_EVENT_MASK_POINTER_MOTION | 
                          XCB_EVENT_MASK_BUTTON_MOTION |
                          XCB_EVENT_MASK_STRUCTURE_NOTIFY;

    const int valwin[] = {eventmask, colormap, 0};
    const int valmask = XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;

    cookie = xcb_create_window_checked(game.xcb.connection,
                                       XCB_COPY_FROM_PARENT,
                                       game.xcb.window,
                                       screen->root,
                                       0, 0,
                                       game.window.width, game.window.height,
                                       10,
                                       XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                       screen->root_visual,
                                       valmask, valwin);

    error = xcb_request_check(game.xcb.connection, cookie);

    if(error != NULL) {
        fprintf(stderr, "Failed to create XCB window!\n");

        window_error_print(error);

        free(error);
        return false;
    }
    
    // Map the window
    cookie = xcb_map_window_checked(game.xcb.connection, game.xcb.window);

    error = xcb_request_check(game.xcb.connection, cookie);

    if(error != NULL) {
        fprintf(stderr, "Failed to map XCB window!\n");

        window_error_print(error);

        free(error);
        return false;
    }

    // Start GLX
    game.gl.window = glXCreateWindow(game.xlib.display, 
                                     fb_config, 
                                     game.xcb.window, 
                                     0);

    if(!game.xcb.window) {
        fprintf(stderr, "Failed to create XCB window!\n");
        
        return false;
    }

    game.gl.drawable = game.gl.window;

    int success = glXMakeContextCurrent(game.xlib.display,
                                       game.gl.drawable,
                                       game.gl.drawable,
                                       game.gl.context);

    if(!success) {
        fprintf(stderr, "Failed to make OpenGL current!\n");

        return false;
    }

    // Set the window's name
    cookie = xcb_change_property_checked(game.xcb.connection,
                                         XCB_PROP_MODE_REPLACE,
                                         game.xcb.window,
                                         XCB_ATOM_WM_NAME,
                                         XCB_ATOM_STRING,
                                         8,
                                         strlen(WN_NAME),
                                         WN_NAME);

    error = xcb_request_check(game.xcb.connection, cookie);

    if(error != NULL) {
        fprintf(stderr, "Failed to rename window!\n");

        window_error_print(error);

        free(error);
        return false;
    }

    return true;
}

// See https://marc.info/?l=freedesktop-xcb&m=129381953404497
// Sadly that is the best result I could find
bool
window_get_close_event(void)
{
    xcb_generic_error_t *error;

    // We need to get WM_PROTOCOLS before we can get the close event
    xcb_intern_atom_cookie_t c_proto = xcb_intern_atom(game.xcb.connection,
                                                       1, 12,
                                                       "WM_PROTOCOLS");
    xcb_intern_atom_reply_t *r_proto = 
                                    xcb_intern_atom_reply(game.xcb.connection,
                                                          c_proto,
                                                          &error);

    // Check for error
    if(error != NULL)
    {
        fprintf(stderr, "Failed to get WM_PROTOCOLS!\n");

        window_error_print(error);

        free(error);
        return false;
    }

    // Get the close event
    xcb_intern_atom_cookie_t c_close = xcb_intern_atom(game.xcb.connection,
                                                       0, 16,
                                                       "WM_DELETE_WINDOW");
    xcb_intern_atom_reply_t *r_close = 
                                    xcb_intern_atom_reply(game.xcb.connection,
                                                          c_close,
                                                          &error);

    // Check for error
    if(error != NULL)
    {
        fprintf(stderr, "Failed to get WM_DELETE_WINDOW!\n");

        window_error_print(error);

        free(error);
        return false;
    }

    // Enable the close event so we can actually receive it
    xcb_void_cookie_t cookie = xcb_change_property(game.xcb.connection,
                                                   XCB_PROP_MODE_REPLACE,
                                                   game.xcb.window,
                                                   r_proto->atom,
                                                   XCB_ATOM_ATOM,
                                                   32,
                                                   1,
                                                   &r_close->atom);

    error = xcb_request_check(game.xcb.connection, cookie);

    if(error != NULL) {
        fprintf(stderr, "Failed to get XCB window close event!\n");

        window_error_print(error);

        free(error);
        return false;
    }


    game.xcb.close_event = r_close->atom;

    return true;
}