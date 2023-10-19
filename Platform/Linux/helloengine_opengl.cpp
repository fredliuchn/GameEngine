#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <xcb/xcb.h>

#include <GL/gl.h> 
#include <GL/glx.h> 
#include <GL/glu.h>

#define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092
typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

// 检查扩展字符串是否存在的帮助程序
static bool isExtensionSupported(const char *extList, const char *extension)
{
  const char *start;
  const char *where, *terminator;
  
  /* 扩展名不应包含空格*/
  where = strchr(extension, ' ');
  if (where || *extension == '\0')
    return false;

  /* 在解析 OpenGL 扩展字符串时要做到万无一失，需要小心。不要被子字符串等所迷惑。 */
  for (start=extList;;) {
    where = strstr(start, extension);

    if (!where)
      break;

    terminator = where + strlen(extension);

    if ( where == start || *(where - 1) == ' ' )
      if ( *terminator == ' ' || *terminator == '\0' )
        return true;

    start = terminator;
  }

  return false;
}

static bool ctxErrorOccurred = false;
static int ctxErrorHandler(Display *dpy, XErrorEvent *ev)
{
    ctxErrorOccurred = true;
    return 0;
}

void DrawAQuad() {
    glClearColor(1.0, 1.0, 1.0, 1.0); 
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 

    glMatrixMode(GL_PROJECTION); 
    glLoadIdentity(); 
    glOrtho(-1., 1., -1., 1., 1., 20.); 

    glMatrixMode(GL_MODELVIEW); 
    glLoadIdentity(); 
    gluLookAt(0., 0., 10., 0., 0., 0., 0., 1., 0.); 

    glBegin(GL_QUADS); 
    glColor3f(1., 0., 0.); 
    glVertex3f(-.75, -.75, 0.); 
    glColor3f(0., 1., 0.); 
    glVertex3f( .75, -.75, 0.); 
    glColor3f(0., 0., 1.); 
    glVertex3f( .75, .75, 0.); 
    glColor3f(1., 1., 0.); 
    glVertex3f(-.75, .75, 0.); 
    glEnd(); 
} 

int main(void) {
    xcb_connection_t    *pConn;
    xcb_screen_t        *pScreen;
    xcb_window_t        window;
    xcb_gcontext_t      foreground;
    xcb_gcontext_t      background;
    xcb_generic_event_t *pEvent;
    xcb_colormap_t colormap;
    uint32_t        mask = 0;
    uint32_t        values[3];
    uint8_t         isQuit = 0;

    char title[] = "Hello, Engine![OpenGL]";
    char title_icon[] = "Hello, Engine! (iconified)";

    Display *display;
    int default_screen;
    GLXContext context;
    GLXFBConfig *fb_configs;
    GLXFBConfig fb_config;
    int num_fb_configs = 0;
    XVisualInfo *vi;
    GLXDrawable drawable;
    GLXWindow glxwindow;
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB;
    const char *glxExts;

    // 获取匹配的 FB 配置
    static int visual_attribs[] =
    {
      GLX_X_RENDERABLE    , True,
      GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
      GLX_RENDER_TYPE     , GLX_RGBA_BIT,
      GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
      GLX_RED_SIZE        , 8,
      GLX_GREEN_SIZE      , 8,
      GLX_BLUE_SIZE       , 8,
      GLX_ALPHA_SIZE      , 8,
      GLX_DEPTH_SIZE      , 24,
      GLX_STENCIL_SIZE    , 8,
      GLX_DOUBLEBUFFER    , True,
      //GLX_SAMPLE_BUFFERS  , 1,
      //GLX_SAMPLES         , 4,
      None
    };

    int glx_major, glx_minor;

    /* 打开 Xlib 显示 */ 
    display = XOpenDisplay(NULL);
    if(!display)
    {
        fprintf(stderr, "Can't open display\n");
        return -1;
    }

    // FBConfigs were added in GLX version 1.3.
    if (!glXQueryVersion(display, &glx_major, &glx_minor) || 
       ((glx_major == 1) && (glx_minor < 3)) || (glx_major < 1))
    {
        fprintf(stderr, "Invalid GLX version\n");
        return -1;
    }

    default_screen = DefaultScreen(display);

    /* 查询帧缓冲区配置 */
    fb_configs = glXChooseFBConfig(display, default_screen, visual_attribs, &num_fb_configs);
    if(!fb_configs || num_fb_configs == 0)
    {
        fprintf(stderr, "glXGetFBConfigs failed\n");
        return -1;
    }

    /* 选择每像素样本最多的 FB 配置/视觉效果 */
    {
        int best_fbc = -1, worst_fbc = -1, best_num_samp = -1, worst_num_samp = 999;

        for (int i=0; i<num_fb_configs; ++i)
        {
            XVisualInfo *vi = glXGetVisualFromFBConfig(display, fb_configs[i]);
            if (vi)
            {
                int samp_buf, samples;
                glXGetFBConfigAttrib(display, fb_configs[i], GLX_SAMPLE_BUFFERS, &samp_buf);
                glXGetFBConfigAttrib(display, fb_configs[i], GLX_SAMPLES, &samples);
      
                printf( "  Matching fbconfig %d, visual ID 0x%lx: SAMPLE_BUFFERS = %d,"
                        " SAMPLES = %d\n", 
                        i, vi -> visualid, samp_buf, samples);

                if (best_fbc < 0 || (samp_buf && samples > best_num_samp))
                    best_fbc = i, best_num_samp = samples;
                if (worst_fbc < 0 || !samp_buf || samples < worst_num_samp)
                    worst_fbc = i, worst_num_samp = samples;
            }
            XFree( vi );
        }

        fb_config = fb_configs[best_fbc];
    }

    /* 获得视觉效果 */
    vi = glXGetVisualFromFBConfig(display, fb_config);
    printf("Chosen visual ID = 0x%lx\n", vi->visualid);

    /* 建立与 X server的连接 */
    pConn = XGetXCBConnection(display);
    if(!pConn)
    {
        XCloseDisplay(display);
        fprintf(stderr, "Can't get xcb connection from display\n");
        return -1;
    }

    /* 获取事件队列所有权 */
    XSetEventQueueOwner(display, XCBOwnsEventQueue);

    /* 查找 XCB 屏幕*/
    xcb_screen_iterator_t screen_iter = 
        xcb_setup_roots_iterator(xcb_get_setup(pConn));
    for(int screen_num = vi->screen;
        screen_iter.rem && screen_num > 0;
        --screen_num, xcb_screen_next(&screen_iter));
    pScreen = screen_iter.data;

    /* 获取根窗口 */
    window = pScreen->root;

    /* 为颜色图创建 XID */
    colormap = xcb_generate_id(pConn);

    xcb_create_colormap(
        pConn,
        XCB_COLORMAP_ALLOC_NONE,
        colormap,
        window,
        vi->visualid 
        );

    /* 创建窗口 */
    window = xcb_generate_id(pConn);
    mask = XCB_CW_EVENT_MASK  | XCB_CW_COLORMAP;
    values[0] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS;
    values[1] = colormap;
    values[2] = 0;
    xcb_create_window (pConn,                   /* connection */
                       XCB_COPY_FROM_PARENT,    /* depth */
                       window,                  /* window ID */
                       pScreen->root,           /* parent window */
                       20, 20,                  /* x, y */
                       640, 480,                /* width, height */
                       10,                      /* boarder width */
                       XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class */
                       vi->visualid,            /* visual */
                       mask, values);           /* masks */

    XFree(vi);

    /* 设置窗口的标题 */
    xcb_change_property(pConn, XCB_PROP_MODE_REPLACE, window,
                XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                strlen(title), title);

    xcb_change_property(pConn, XCB_PROP_MODE_REPLACE, window,
                XCB_ATOM_WM_ICON_NAME, XCB_ATOM_STRING, 8,
                strlen(title_icon), title_icon);

    xcb_map_window(pConn, window);

    xcb_flush(pConn);

    /* 获取默认屏幕的GLX扩展列表 */
    glxExts = glXQueryExtensionsString(display, default_screen);

    /* NOTE: 在调用glXGetProcAddressARB之前，没有必要创建当前上下文 */
    glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)
           glXGetProcAddressARB( (const GLubyte *) "glXCreateContextAttribsARB" );

    /* Create OpenGL context */
    ctxErrorOccurred = false;
    int (*oldHandler)(Display*, XErrorEvent*) =
        XSetErrorHandler(&ctxErrorHandler);

    if (!isExtensionSupported(glxExts, "GLX_ARB_create_context") ||
       !glXCreateContextAttribsARB )
    {
        printf( "glXCreateContextAttribsARB() not found"
            " ... using old-style GLX context\n" );
        context = glXCreateNewContext(display, fb_config, GLX_RGBA_TYPE, 0, True);
        if(!context)
        {
            fprintf(stderr, "glXCreateNewContext failed\n");
            return -1;
        }
    }
    else
    {
        int context_attribs[] =
          {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
            GLX_CONTEXT_MINOR_VERSION_ARB, 0,
            None
          };

        printf( "Creating context\n" );
        context = glXCreateContextAttribsARB(display, fb_config, 0,
                                          True, context_attribs );

        XSync(display, False);
        if (!ctxErrorOccurred && context)
          printf( "Created GL 3.0 context\n" );
        else
        {
          /* GLX_CONTEXT_MAJOR_VERSION_ARB = 1 */
          context_attribs[1] = 1;
          /* GLX_CONTEXT_MINOR_VERSION_ARB = 0 */
          context_attribs[3] = 0;

          ctxErrorOccurred = false;

          printf( "Failed to create GL 3.0 context"
                  " ... using old-style GLX context\n" );
          context = glXCreateContextAttribsARB(display, fb_config, 0, 
                                            True, context_attribs );
        }
    }

    XSync(display, False);

    XSetErrorHandler(oldHandler);

    if (ctxErrorOccurred || !context)
    {
        printf( "Failed to create an OpenGL context\n" );
        return -1;
    }

    /* 验证上下文是直接上下文 */
    if (!glXIsDirect (display, context))
    {
        printf( "Indirect GLX rendering context obtained\n" );
    }
    else
    {
        printf( "Direct GLX rendering context obtained\n" );
    }

    /* Create GLX Window */
    glxwindow = 
            glXCreateWindow(
                display,
                fb_config,
                window,
                0
                );

    if(!window)
    {
        xcb_destroy_window(pConn, window);
        glXDestroyContext(display, context);

        fprintf(stderr, "glXDestroyContext failed\n");
        return -1;
    }

    drawable = glxwindow;

    /* 使 OpenGL 上下文成为当前上下文 */
    if(!glXMakeContextCurrent(display, drawable, drawable, context))
    {
        xcb_destroy_window(pConn, window);
        glXDestroyContext(display, context);

        fprintf(stderr, "glXMakeContextCurrent failed\n");
        return -1;
    }


    while(!isQuit && (pEvent = xcb_wait_for_event(pConn))) {
        switch(pEvent->response_type & ~0x80) {
        case XCB_EXPOSE:
            {       
                DrawAQuad();
                glXSwapBuffers(display, drawable);
            }
            break;
        case XCB_KEY_PRESS:
            isQuit = 1;
            break;
        }
        free(pEvent);
    }


    /* Cleanup */
    xcb_disconnect(pConn);

    return 0;
}
    