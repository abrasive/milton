// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license


static b32 g_cursor_count = 0;

void cursor_hide()
{
#if defined(_WIN32)
    while (g_cursor_count >= 0)
    {
        ShowCursor(FALSE);
        g_cursor_count--;
    }
#else
    // TODO: haven't checked if this works.
    int lvl = SDL_ShowCursor(-1);
    if ( lvl >= 0 )
    {
        mlt_assert ( lvl == 1 );
        int res = SDL_ShowCursor(0);
        if (res < 0)
        {
            INVALID_CODE_PATH;
        }
    }
#endif
}

void cursor_show()
{
#if defined(_WIN32)
    while (g_cursor_count < 0)
    {
        ShowCursor(TRUE);
        g_cursor_count++;
    }
#else
    int lvl = SDL_ShowCursor(-1);
    if ( lvl < 0 )
    {
        mlt_assert ( lvl == -1 );
        SDL_ShowCursor(1);
    }
#endif
}

SDL_Cursor* g_curr_cursor = NULL;
static void cursor_set_and_show(SDL_Cursor* cursor)
{
    if (g_curr_cursor != cursor)
    {
        g_curr_cursor = cursor;
        SDL_SetCursor(cursor);
        cursor_show();
    }
}

LayoutType get_current_keyboard_layout()
{
    LayoutType layout = LayoutType_QWERTY;  // Default to QWERTY bindings.

    char keys[] =
    {
        (char)SDL_GetKeyFromScancode(SDL_SCANCODE_Q),
        (char)SDL_GetKeyFromScancode(SDL_SCANCODE_R),
        (char)SDL_GetKeyFromScancode(SDL_SCANCODE_Y),
        '\0',
    };

    if (strcmp(keys, "qry") == 0)
    {
        layout = LayoutType_QWERTY;
    }
    else if (strcmp(keys, "ary") == 0)
    {
        layout = LayoutType_AZERTY;
    }
    else if (strcmp(keys, "qrz") == 0)
    {
        layout = LayoutType_QWERTZ;
    }
    else if (strcmp(keys, "q,f"))
    {
        layout = LayoutType_DVORAK;
    }
    else if (strcmp(keys, "qwj") == 0)
    {
        layout = LayoutType_COLEMAK;
    }

    return layout;
}

void panning_update(PlatformState* platform_state)
{
    auto reset_pan_start = [platform_state]()
    {
        platform_state->pan_start = platform_state->pointer;
        platform_state->pan_point = platform_state->pan_start;  // No huge pan_delta at beginning of pan.
    };

    if (platform_state->waiting_for_pan_input)
    {
        if (platform_state->is_pointer_down || platform_state->is_middle_button_down)
        {
            platform_state->waiting_for_pan_input = false;
            reset_pan_start();
        }
        // Space cancels waiting
        if (platform_state->is_space_down)
        {
            platform_state->waiting_for_pan_input = false;
        }
    }
    else
    {
        if (platform_state->is_panning)
        {
            if ((!platform_state->is_pointer_down && !platform_state->is_space_down) ||
                !platform_state->is_pointer_down)
            {
                platform_state->is_panning = false;
            }
            else
            {
                platform_state->pan_point = platform_state->pointer;
            }
        }
        else
        {
            if ((platform_state->is_space_down && platform_state->is_pointer_down) ||
                platform_state->is_middle_button_down)
            {
                platform_state->is_panning = true;
                reset_pan_start();
            }
        }
    }
}

MiltonInput sdl_event_loop(MiltonState* milton_state, PlatformState* platform_state)
{
    MiltonInput milton_input = {};

    b32 pointer_up = false;

    v2i input_point = {};

    platform_state->num_pressure_results = 0;
    platform_state->num_point_results = 0;
    platform_state->keyboard_layout = get_current_keyboard_layout();

    i32 input_flags = (i32)MiltonInputFlags_NONE;

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        //ImGui_ImplSdl_ProcessEvent(&event);
        ImGui_ImplSdlGL3_ProcessEvent(&event);

        SDL_Keymod keymod = SDL_GetModState();
        platform_state->is_ctrl_down = (keymod & KMOD_LCTRL) | (keymod & KMOD_RCTRL);
        platform_state->is_shift_down = (keymod & KMOD_SHIFT);

#if 0
        if ( (keymod & KMOD_ALT) )
        {
            input_flags |= MiltonInputFlags_CHANGE_MODE;
            milton_input.mode_to_set = MiltonMode_EYEDROPPER;
        }
#endif


#if defined(_MSC_VER)
#pragma warning (push)
#pragma warning (disable : 4061)
#endif
        switch (event.type)
        {
        case SDL_QUIT:
            cursor_show();
            milton_try_quit(milton_state);
            break;
        case SDL_SYSWMEVENT:
            {
                f32 pressure = NO_PRESSURE_INFO;
                SDL_SysWMEvent sysevent = event.syswm;
                EasyTabResult er = EASYTAB_EVENT_NOT_HANDLED;
                switch(sysevent.msg->subsystem)
                {
#if defined(_WIN32)
                case SDL_SYSWM_WINDOWS:
                    er = EasyTab_HandleEvent(sysevent.msg->msg.win.hwnd,
                                             sysevent.msg->msg.win.msg,
                                             sysevent.msg->msg.win.lParam,
                                             sysevent.msg->msg.win.wParam);
                    break;
#elif defined(__linux__)
                case SDL_SYSWM_X11:
                    er = EasyTab_HandleEvent(&sysevent.msg->msg.x11.event);
                    break;
#elif defined(__MACH__)
                case SDL_SYSWM_COCOA:
                    // SDL does not implement this in the version we're using.
                    // See platform_OSX_SDL_hooks.(h|m) for our SDL hack.
                    break;
#endif
                default:
                    break;  // Are we in Wayland yet?

                }
                if (er == EASYTAB_OK)
                {
                    if (EasyTab->Erasing != platform_state->is_eraser_near) {
                        if (EasyTab->Erasing)
                            milton_input.mode_to_set = MiltonMode_ERASER;
                        else
                            milton_input.mode_to_set = MiltonMode_PEN;

                        input_flags |= MiltonInputFlags_CHANGE_MODE;
                        platform_state->is_eraser_near = EasyTab->Erasing;
                    }

                    b32 got_pen = false;
                    // Pen in use but not drawing
                    b32 was_pen_down = EasyTab->PenInProximity && !platform_state->is_pointer_down;
                    for (int pi = 0; pi < EasyTab->NumPackets; ++pi)
                    {
                        bool is_down = EasyTab->Pressure[pi] > 0 && EasyTab->PenInProximity;
                        if (is_down)
                        {
                            got_pen = true;
                            v2i point = { EasyTab->PosX[pi], EasyTab->PosY[pi] };
                            if (point.x >= 0 && point.y >= 0)
                            {
                                if (platform_state->num_point_results < MAX_INPUT_BUFFER_ELEMS)
                                {
                                    milton_input.points[platform_state->num_point_results++] = point;
                                }
                                if (platform_state->num_pressure_results < MAX_INPUT_BUFFER_ELEMS)
                                {
                                    milton_input.pressures[platform_state->num_pressure_results++] = EasyTab->Pressure[pi];
                                }
                            }
                        }
                    }

                    if (got_pen)
                    {
                        platform_state->is_pointer_down = true;
                    }
                    if (EasyTab->NumPackets > 0)
                    {
                        v2i point = { EasyTab->PosX[EasyTab->NumPackets-1], EasyTab->PosY[EasyTab->NumPackets-1] };
                        input_flags |= MiltonInputFlags_HOVERING;

                        platform_state->pointer = point;
                    }
                    // Wasn't drawing, and now we got a result with pressure>0
                    b32 pen_touched_tab = !was_pen_down && EasyTab->PenInProximity && (platform_state->num_pressure_results>0);
                }
            } break;
        case SDL_MOUSEBUTTONDOWN:
            {
                if (event.button.windowID != platform_state->window_id)
                {
                    break;
                }

                if (event.button.button == SDL_BUTTON_LEFT ||
                    event.button.button == SDL_BUTTON_MIDDLE ||
                    event.button.button == SDL_BUTTON_RIGHT)
                {
                    if (!ImGui::GetIO().WantCaptureMouse)
                    {
                        v2i point = { event.button.x, event.button.y };

                        if (!platform_state->is_panning && point.x >= 0 && point.y > 0)
                        {
                            input_flags |= MiltonInputFlags_CLICK;
                            milton_input.click = point;

                            platform_state->is_pointer_down = true;
                            platform_state->pointer = point;
                            platform_state->is_middle_button_down = (event.button.button == SDL_BUTTON_MIDDLE);

                            if (platform_state->num_point_results < MAX_INPUT_BUFFER_ELEMS)
                            {
                                milton_input.points[platform_state->num_point_results++] = point;
                            }
                            if (platform_state->num_pressure_results < MAX_INPUT_BUFFER_ELEMS)
                            {
                                milton_input.pressures[platform_state->num_pressure_results++] = NO_PRESSURE_INFO;
                            }
                        }
                    }
                    else
                    {
                        platform_state->force_next_frame = true;
                    }
                }
            } break;
        case SDL_MOUSEBUTTONUP:
            {
                if (event.button.windowID != platform_state->window_id)
                {
                    break;
                }
                if (event.button.button == SDL_BUTTON_LEFT ||
                    event.button.button == SDL_BUTTON_MIDDLE ||
                    event.button.button == SDL_BUTTON_RIGHT)
                {
                    if (event.button.button == SDL_BUTTON_MIDDLE)
                    {
                        platform_state->is_middle_button_down = false;
                    }
                    pointer_up = true;
                    input_flags |= MiltonInputFlags_CLICKUP;
                    input_flags |= MiltonInputFlags_END_STROKE;
                }
            } break;
        case SDL_MOUSEMOTION:
            {
                if ( event.motion.windowID != platform_state->window_id )
                {
                    break;
                }
                input_point = { event.motion.x, event.motion.y };

                platform_state->pointer = input_point;

                // Check if it is empty. In case the wacom driver craps out, or
                // anything goes wrong (like the event queue overflowing ;))
                // then we default to receiving WM_MOUSEMOVE.
                // If we catch a single point, then it's fine. It will get filtered out in milton_stroke_input
                b32 is_empty = platform_state->num_point_results == 0;

                // Only get mouse info when wacom is not in use, or if EasyTab is NULL.
                if (EasyTab == NULL || !EasyTab->PenInProximity || is_empty)
                {
                    if (platform_state->is_pointer_down)
                    {
                        if (!platform_state->is_panning)
                        {
                            if (input_point.x >= 0 && input_point.y >= 0)
                            {
                                if (platform_state->num_point_results < MAX_INPUT_BUFFER_ELEMS)
                                {
                                    milton_input.points[platform_state->num_point_results++] = input_point;
                                }
                                if (platform_state->num_pressure_results < MAX_INPUT_BUFFER_ELEMS)
                                {
                                    milton_input.pressures[platform_state->num_pressure_results++] = NO_PRESSURE_INFO;
                                }
                            }
                        }
                        input_flags &= ~MiltonInputFlags_HOVERING;
                    }
                    else
                    {
                        input_flags |= MiltonInputFlags_HOVERING;
                    }
                }
                break;
            }
        case SDL_MOUSEWHEEL:
            {
                if ( event.wheel.windowID != platform_state->window_id )
                {
                    break;
                }
                if ( !ImGui::GetIO().WantCaptureMouse )
                {
                    milton_input.scale += event.wheel.y;
                }

                break;
            }
        case SDL_KEYDOWN:
            {
                if ( event.wheel.windowID != platform_state->window_id )
                {
                    break;
                }

                SDL_Keycode keycode = event.key.keysym.sym;

                // Actions accepting key repeats.
                {
                    if (keycode == SDLK_LEFTBRACKET)
                    {
                        milton_decrease_brush_size(milton_state);
                        milton_state->hover_flash_ms = (i32)SDL_GetTicks();
                        milton_state->flags |= MiltonStateFlags_BRUSH_HOVER_FLASHING;
                    }
                    else if (keycode == SDLK_RIGHTBRACKET)
                    {
                        milton_increase_brush_size(milton_state);
                        milton_state->hover_flash_ms = (i32)SDL_GetTicks();
                        milton_state->flags |= MiltonStateFlags_BRUSH_HOVER_FLASHING;
                    }
                    if (platform_state->is_ctrl_down)
                    {
                        if ((platform_state->keyboard_layout == LayoutType_QWERTZ && (keycode == SDLK_ASTERISK)) ||
                            (platform_state->keyboard_layout == LayoutType_AZERTY && (keycode == SDLK_EQUALS)) ||
                            (platform_state->keyboard_layout == LayoutType_QWERTY && (keycode == SDLK_EQUALS)) ||
                            keycode == SDLK_PLUS)
                        {
                            milton_input.scale++;
                        }
                        if ((platform_state->keyboard_layout == LayoutType_AZERTY && (keycode == SDLK_6)) ||
                            keycode == SDLK_MINUS)
                        {
                            milton_input.scale--;
                        }
                        if (keycode == SDLK_z)
                        {
                            if (platform_state->is_shift_down)
                            {
                                input_flags |= MiltonInputFlags_REDO;
                            }
                            else
                            {
                                input_flags |= MiltonInputFlags_UNDO;
                            }
                        }
                    }

                }
                if (event.key.repeat)
                {
                    break;
                }

                // Stop stroking when any key is hit
                input_flags |= MiltonInputFlags_END_STROKE;

                if (keycode == SDLK_SPACE)
                {
                    platform_state->is_space_down = true;
                    // Stahp
                }
                if (platform_state->is_ctrl_down)
                {  // Ctrl-KEY with no key repeats.
                    if (keycode == SDLK_e)
                    {
                        input_flags |= MiltonInputFlags_CHANGE_MODE;
                        milton_input.mode_to_set = MiltonMode_EXPORTING;
                    }
                    if (keycode == SDLK_q)
                    {
                        milton_try_quit(milton_state);
                    }
                }
                else
                {
                    if (!ImGui::GetIO().WantCaptureMouse )
                    {
                        if (keycode == SDLK_e)
                        {
                            input_flags |= MiltonInputFlags_CHANGE_MODE;
                            milton_input.mode_to_set = MiltonMode_ERASER;
                        }
                        else if (keycode == SDLK_b)
                        {
                            input_flags |= MiltonInputFlags_CHANGE_MODE;
                            milton_input.mode_to_set = MiltonMode_PEN;
                        }
                        else if (keycode == SDLK_i)
                        {
                            input_flags |= MiltonInputFlags_CHANGE_MODE;
                            milton_input.mode_to_set = MiltonMode_EYEDROPPER;
                        }
                        else if (keycode == SDLK_TAB)
                        {
                            gui_toggle_visibility(milton_state);
                        }
                        else if (keycode == SDLK_F1)
                        {
                            gui_toggle_help(milton_state->gui);
                        }
                        else if (keycode == SDLK_1)
                        {
                            milton_set_pen_alpha(milton_state, 0.1f);
                        }
                        else if (keycode == SDLK_2)
                        {
                            milton_set_pen_alpha(milton_state, 0.2f);
                        }
                        else if (keycode == SDLK_3)
                        {
                            milton_set_pen_alpha(milton_state, 0.3f);
                        }
                        else if (keycode == SDLK_4)
                        {
                            milton_set_pen_alpha(milton_state, 0.4f);
                        }
                        else if (keycode == SDLK_5)
                        {
                            milton_set_pen_alpha(milton_state, 0.5f);
                        }
                        else if (keycode == SDLK_6)
                        {
                            milton_set_pen_alpha(milton_state, 0.6f);
                        }
                        else if (keycode == SDLK_7)
                        {
                            milton_set_pen_alpha(milton_state, 0.7f);
                        }
                        else if (keycode == SDLK_8)
                        {
                            milton_set_pen_alpha(milton_state, 0.8f);
                        }
                        else if (keycode == SDLK_9)
                        {
                            milton_set_pen_alpha(milton_state, 0.9f);
                        }
                        else if (keycode == SDLK_0)
                        {
                            milton_set_pen_alpha(milton_state, 1.0f);
                        }
                    }
#if MILTON_DEBUG
                    if (keycode == SDLK_F4)
                    {
                        milton_log("[DEBUG]: Switching to %s renderer.\n",
                                   milton_state->DEBUG_sse2_switch ? "SSE" : "slow");
                        profiler_reset();
                        milton_state->DEBUG_sse2_switch = !milton_state->DEBUG_sse2_switch;
                    }
#endif
#if MILTON_ENABLE_PROFILING
                    if (keycode == SDLK_BACKQUOTE)
                    {
                        milton_state->viz_window_visible = !milton_state->viz_window_visible;
                    }
#endif
                }

                break;
            }
        case SDL_KEYUP:
            {
                if (event.key.windowID != platform_state->window_id) {
                    break;
                }

                SDL_Keycode keycode = event.key.keysym.sym;

                if (keycode == SDLK_SPACE)
                {
                    platform_state->is_space_down = false;
                }
            } break;
        case SDL_WINDOWEVENT:
            {
                if (platform_state->window_id != event.window.windowID)
                {
                    break;
                }
                switch (event.window.event)
                {
                    // Just handle every event that changes the window size.
                case SDL_WINDOWEVENT_MOVED:
                    platform_state->num_point_results = 0;
                    platform_state->num_pressure_results = 0;
                    platform_state->is_pointer_down = false;
                    break;
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    platform_state->width = event.window.data1;
                    platform_state->height = event.window.data2;
                    input_flags |= MiltonInputFlags_FULL_REFRESH;
                    glViewport(0, 0, platform_state->width, platform_state->height);
                    break;
                case SDL_WINDOWEVENT_LEAVE:
                    if ( event.window.windowID != platform_state->window_id )
                    {
                        break;
                    }
                    cursor_show();
                    break;
                    // --- A couple of events we might want to catch later...
                case SDL_WINDOWEVENT_ENTER:
                    {
                    } break;
                    break;
                case SDL_WINDOWEVENT_FOCUS_GAINED:
                    break;
                default:
                    break;
                }
            } break;
        default:
            break;
        }
#if defined(_MSC_VER)
#pragma warning (pop)
#endif
        if ( platform_state->should_quit )
        {
            break;
        }
    }  // ---- End of SDL event loop

    if (pointer_up)
    {
        // Add final point
        if (!platform_state->is_panning && platform_state->is_pointer_down)
        {
            input_flags |= MiltonInputFlags_END_STROKE;
            input_point = { event.button.x, event.button.y };

            if (platform_state->num_point_results < MAX_INPUT_BUFFER_ELEMS)
            {
                milton_input.points[platform_state->num_point_results++] = input_point;
            }
            // Start drawing hover as soon as we stop the stroke.
            input_flags |= MiltonInputFlags_HOVERING;
        }
        platform_state->is_pointer_down = false;

        platform_state->num_point_results = 0;
    }

    milton_input.flags = (MiltonInputFlags)input_flags;

    return milton_input;
}

// ---- milton_main

int milton_main()
{
    // TODO: windows scaling support
#if 1
#if defined(_WIN32)
    if (!SetProcessDPIAware())  // This function is only present in Windows versions higher than Vista.
    {
        milton_log("Could not set this process as DPI aware.\n");
    }
#endif
#endif
    // Note: Possible crash regarding SDL_main entry point.
    // Note: Event handling, File I/O and Threading are initialized by default
    SDL_Init(SDL_INIT_VIDEO);

    PlatformState platform_state = {};

    PlatformPrefs prefs = {};

    milton_prefs_load(&prefs);

    if (prefs.width == 0 || prefs.height == 0)
    {
        platform_state.width = 1280;
        platform_state.height = 800;
    }
    else
    {
        platform_state.width = prefs.width;
        platform_state.height = prefs.height;
    }

    platform_state.keyboard_layout = get_current_keyboard_layout();

    i32 gl_version_major = 3;
    i32 gl_version_minor = 2;

    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, gl_version_major);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, gl_version_minor);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // TODO: This has a different meaning than "multisampled textures."
    #if MULTISAMPLED_TEXTURES
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, MSAA_NUM_SAMPLES);
    #endif


    SDL_Window* window = SDL_CreateWindow("Milton",
                                          SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          platform_state.width, platform_state.height,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!window)
    {
        milton_log("[ERROR] -- Exiting. SDL could not create window\n");
        exit(EXIT_FAILURE);
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    if (!gl_context)
    {
        milton_fatal("Could not create OpenGL context\n");
    }

    SDL_GL_SetSwapInterval(1);

    int actual_major = 0;
    int actual_minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &actual_major);
    glGetIntegerv(GL_MINOR_VERSION, &actual_minor);
    if (actual_major < gl_version_major ||
        (actual_major == gl_version_major && actual_minor < gl_version_minor))
    {
        milton_die_gracefully("This graphics driver does not support OpenGL 3.2+");
    }
    milton_log("Created OpenGL context with version %s\n", glGetString(GL_VERSION));
    milton_log("    and GLSL %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    // ==== Initialize milton
    //  Total (static) memory requirement for Milton
    size_t sz_root_arena = (size_t)10 * 1024 * 1024;

    // Using platform_allocate because stdlib calloc will be really slow.
    void* big_chunk_of_memory = platform_allocate_bounded_memory(sz_root_arena);

    if (!big_chunk_of_memory)
    {
        milton_fatal("Could not allocate bounded virtual memory for Milton.\n");
    }

    Arena root_arena = arena_init(big_chunk_of_memory, sz_root_arena);

    MiltonState* milton_state = arena_alloc_elem(&root_arena, MiltonState);
    b32 has_sample_shading = false;

    if (!gl_load(&has_sample_shading))
    {
        milton_die_gracefully("Milton could not load the necessary OpenGL functionality. Exiting.");
    }

    // Initialize milton_state
    {
        milton_state->root_arena = &root_arena;

        milton_state->render_data = arena_alloc_elem(milton_state->root_arena, RenderData);
        if (has_sample_shading)
        {
            milton_state->render_data->flags |= RenderDataFlags_HAS_SAMPLE_SHADING;
        }
        milton_init(milton_state, platform_state.width, platform_state.height);
    }

    // Ask for native events to poll tablet events.
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);

    SDL_SysWMinfo sysinfo;
    SDL_VERSION(&sysinfo.version);

#if defined(_MSC_VER)
#pragma warning (push, 0)
#endif
    if (SDL_GetWindowWMInfo( window, &sysinfo ))
    {
        switch(sysinfo.subsystem)
        {
#if defined(_WIN32)
        case SDL_SYSWM_WINDOWS:
            {
                { // Handle the case where the window was too big for the screen.
                    HWND hwnd = sysinfo.info.win.window;
                    RECT res_rect;
                    RECT win_rect;
                    HWND dhwnd = GetDesktopWindow();
                    GetWindowRect(dhwnd, &res_rect);
                    GetClientRect(hwnd, &win_rect);

                    platform_state.hwnd = hwnd;

                    i32 snap_threshold = 300;
                    if (win_rect.right  != platform_state.width ||
                        win_rect.bottom != platform_state.height ||
                        // Also maximize if the size is large enough to "snap"
                        (win_rect.right + snap_threshold >= res_rect.right &&
                         win_rect.left + snap_threshold >= res_rect.left))
                    {
                        // Our prefs weren't right. Let's maximize.
                        SetWindowPos(hwnd, HWND_TOP, 20,20, win_rect.right-20, win_rect.bottom -20, SWP_SHOWWINDOW);
                        platform_state.width = win_rect.right - 20;
                        platform_state.height = win_rect.bottom - 20;
                        ShowWindow(hwnd, SW_MAXIMIZE);
                    }
                }
                // Load EasyTab
                EasyTab_Load(platform_state.hwnd);
                break;
            }
#elif defined(__linux__)
        case SDL_SYSWM_X11:
            EasyTab_Load(sysinfo.info.x11.display, sysinfo.info.x11.window);
            break;
#endif
        default:
            break;
        }
    }
    else
    {
        milton_die_gracefully("Can't get system info!\n");
    }
#if defined(_MSC_VER)
#pragma warning (pop)
#endif

    milton_resize_and_pan(milton_state, {}, {platform_state.width, platform_state.height});

    platform_state.window_id = SDL_GetWindowID(window);

    // Init ImGUI
    //ImGui_ImplSdl_Init(window);
    ImGui_ImplSdlGL3_Init(window);

#if defined(_WIN32)
    {  // Load icon (Win32)
        int si = sizeof(HICON);
        HINSTANCE handle = GetModuleHandle(nullptr);
        PATH_CHAR icopath[MAX_PATH] = L"milton_icon.ico";
        platform_fname_at_exe(icopath, MAX_PATH);
        HICON icon = (HICON)LoadImageW(NULL, icopath, IMAGE_ICON, /*W*/0, /*H*/0,
                                       LR_LOADFROMFILE | LR_DEFAULTSIZE | LR_SHARED);
        if (icon != NULL)
        {
            SendMessage(platform_state.hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
        }
    }
    {  // Set brush HW cursor
        size_t w = (size_t)GetSystemMetrics(SM_CXCURSOR);
        size_t h = (size_t)GetSystemMetrics(SM_CYCURSOR);

        size_t arr_sz = (w*h+7) / 8;

        char* andmask = arena_alloc_array(&root_arena, arr_sz, char);
        char* xormask = arena_alloc_array(&root_arena, arr_sz, char);

        i32 counter = 0;
        {
            size_t cx = w/2;
            size_t cy = h/2;
            for (size_t j = 0; j < h; ++j)
            {
                for (size_t i = 0; i < w; ++i)
                {
                    size_t dist = (i-cx)*(i-cx) + (j-cy)*(j-cy);

                    // 32x32 default;
                    i64 girth = 3; // girth of cursor in pixels
                    size_t radius = 8;
                    if (w == 32 && h == 32)
                    {
                        // 32x32
                    }
                    else if (w == 64 && h == 64)
                    {
                        girth *= 2;
                        radius *= 2;
                    }
                    else
                    {
                        INVALID_CODE_PATH;
                    }
                    i64 diff        = (i64)(dist - SQUARE(radius));
                    b32 in_white = diff < SQUARE(girth-0.5f) && diff > -SQUARE(girth-0.5f);
                    diff = (i64)(dist - SQUARE(radius+1));
                    b32 in_black = diff < SQUARE(girth) && diff > -SQUARE(girth);

                    size_t idx = j*w + i;

                    size_t ai = idx / 8;
                    size_t bi = idx % 8;

                    // This code block for windows CreateCursor
#if 0
                    if (incircle &&
                        // Cross-hair effect. Only pixels inside half-radius bands get drawn.
                        (i > cx-radius/2 && i < cx+radius/2 || j > cy-radius/2 && j < cy+radius/2))
                    {
                        if (toggle_black)
                        {
                            xormask[ai] |= (1 << (7 - bi));
                        }
                        else
                        {
                            xormask[ai] &= ~(1 << (7 - bi));
                            xormask[ai] &= ~(1 << (7 - bi));
                        }
                        toggle_black = !toggle_black;
                    }
                    else
                    {
                        andmask[ai] |= (1 << (7 - bi));
                    }
#endif
                    // SDL code block
                    if (in_white)
                        // Cross-hair effect. Only pixels inside half-radius bands get drawn.
                        /* (i > cx-radius/2 && i < cx+radius/2 || j > cy-radius/2 && j < cy+radius/2)) */
                    {
                        andmask[ai] &= ~(1 << (7 - bi));  // White
                        xormask[ai] |= (1 << (7 - bi));
                    }
                    else if (in_black)
                    {
                        xormask[ai] |= (1 << (7 - bi));  // Black
                        andmask[ai] |= (1 << (7 - bi));

                    }
                    else
                    {
                        andmask[ai] &= ~(1 << (7 - bi));     // Transparent
                        xormask[ai] &= ~(1 << (7 - bi));
                    }
                }
            }
        }
        //platform_state.hcursor = CreateCursor(/*HINSTANCE*/ 0,
        //                                      /*xHotSpot*/(int)(w/2),
        //                                      /*yHotSpot*/(int)(h/2),
        //                                      /* nWidth */(int)w,
        //                                      /* nHeight */(int)h,
        //                                      (VOID*)andmask,
        //                                      (VOID*)xormask);

        platform_state.cursor_brush = SDL_CreateCursor((Uint8*)andmask,
                                                       (Uint8*)xormask,
                                                       (int)w,
                                                       (int)h,
                                                       /*xHotSpot*/(int)(w/2),
                                                       /*yHotSpot*/(int)(h/2));

    }
#endif


    // ImGui setup
    {
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = NULL;  // Don't save any imgui.ini file
        PATH_CHAR fname[MAX_PATH] = TO_PATH_STR("Carlito.ttf");
        platform_fname_at_exe(fname, MAX_PATH); // TODO: check that this works again
        FILE* fd = platform_fopen(fname, TO_PATH_STR("rb"));

        if (fd)
        {
            size_t  ttf_sz = 0;
            void*   ttf_data = NULL;
            //ImFont* im_font =  io.Fonts->ImFontAtlas::AddFontFromFileTTF("carlito.ttf", 14);
            // Load file to memory
            if (fseek(fd, 0, SEEK_END) == 0)
            {
                long ttf_sz_long = ftell(fd);
                if (ttf_sz_long != -1)
                {
                    ttf_sz = (size_t)ttf_sz_long;
                    if (fseek(fd, 0, SEEK_SET) == 0)
                    {
                        ttf_data = ImGui::MemAlloc(ttf_sz);
                        if (ttf_data)
                        {
                            if (fread(ttf_data, 1, ttf_sz, fd) == ttf_sz)
                            {
                                ImFont* im_font = io.Fonts->ImFontAtlas::AddFontFromMemoryTTF(ttf_data, (int)ttf_sz, 14);
                            }
                            else
                            {
                                milton_log("WARNING: Error reading TTF file");
                            }
                        }
                        else
                        {
                            milton_log("WARNING: could not allocate data for font!");
                        }
                    }
                }
            }
            fclose(fd);
        }
    }
    // Initalize system cursors
    {
        platform_state.cursor_default   = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
        platform_state.cursor_hand      = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
        platform_state.cursor_crosshair = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
        platform_state.cursor_sizeall   = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);

        cursor_set_and_show(platform_state.cursor_default);
    }

    // ---- Main loop ----

    while (!platform_state.should_quit)
    {
        PROFILE_GRAPH_PUSH(system);
        PROFILE_GRAPH_BEGIN(polling);

        ImGuiIO& imgui_io = ImGui::GetIO();

        MiltonInput milton_input = sdl_event_loop(milton_state, &platform_state);

        panning_update(&platform_state);

        if (!platform_state.is_panning)
        {
            milton_input.flags |= MiltonInputFlags_HOVERING;
            milton_input.hover_point = platform_state.pointer;
        }

        static b32 first_run = true;
        if (first_run)
        {
            first_run = false;
            milton_input.flags = MiltonInputFlags_FULL_REFRESH;
            milton_state->gui->flags |= MiltonGuiFlags_NEEDS_REDRAW;
        }

        {
            int x = 0;
            int y = 0;
            SDL_GetMouseState(&x, &y);

            // Handle system cursor and platform state related to current_mode
            if (platform_state.is_panning)
            {
                cursor_set_and_show(platform_state.cursor_sizeall);
            }
            else if (milton_state->current_mode == MiltonMode_EXPORTING)
            {
                cursor_set_and_show(platform_state.cursor_crosshair);
                platform_state.was_exporting = true;
            }
            else if (platform_state.was_exporting)
            {
                cursor_set_and_show(platform_state.cursor_default);
                platform_state.was_exporting = false;
            }
            else if (milton_state->current_mode == MiltonMode_EYEDROPPER)
            {
                cursor_set_and_show(platform_state.cursor_crosshair);
                platform_state.is_pointer_down = false;
            }
            else if ( milton_state->gui->visible &&
                      is_inside_rect_scalar(get_bounds_for_picker_and_colors(&milton_state->gui->picker), x,y) )
            {
                cursor_set_and_show(platform_state.cursor_default);
            }
            else if ( ImGui::GetIO().WantCaptureMouse )
            {
                cursor_set_and_show(platform_state.cursor_default);
            }
            else if (milton_state->current_mode == MiltonMode_PEN || milton_state->current_mode == MiltonMode_ERASER)
            {
                cursor_set_and_show(platform_state.cursor_brush);
            }
            else if (milton_state->current_mode != MiltonMode_PEN || milton_state->current_mode != MiltonMode_ERASER)
            {
                cursor_hide();
            }

            // Show resize icon
            int pad = 20;
            if (x > milton_state->view->screen_size.w - pad   ||
                x < pad                                       ||
                y > milton_state->view->screen_size.h - pad   ||
                y < pad)
            {
                cursor_set_and_show(platform_state.cursor_default);
            }
        }

        // IN OSX: SDL polled all events, we get all the pressure inputs from our hook
#if defined(__MACH__)
        //mlt_assert( platform_state.num_pressure_results == 0 );
        int num_polled_pressures = 0;
        float* polled_pressures = milton_osx_poll_pressures(&num_polled_pressures);
        if ( num_polled_pressures )
        {
            for (int i = num_polled_pressures - 1; i >= 0; --i)
            {
                milton_input.pressures[platform_state.num_pressure_results++] = polled_pressures[i];
            }
        }
#endif

        i32 input_flags = (i32)milton_input.flags;

        ImGui_ImplSdlGL3_NewFrame();
        // Clear our pointer input because we captured an ImGui widget!
        if (ImGui::GetIO().WantCaptureMouse)
        {
            platform_state.num_point_results = 0;
            platform_state.is_pointer_down = false;
            input_flags |= MiltonInputFlags_IMGUI_GRABBED_INPUT;
        }

        milton_imgui_tick(&milton_input, &platform_state, milton_state);

        // Clear pan delta if we are zooming
        if (milton_input.scale != 0)
        {
            milton_input.pan_delta = {};
            input_flags |= MiltonInputFlags_FAST_DRAW;
        }
        else if (platform_state.is_panning)
        {
            input_flags |= MiltonInputFlags_PANNING;
            platform_state.num_point_results = 0;
        }

#if 0
        milton_log ("#Pressure results: %d\n", num_pressure_results);
        milton_log ("#   Point results: %d\n", num_point_results);
#endif

        if ( platform_state.num_pressure_results < platform_state.num_point_results )
        {
            platform_state.num_point_results = platform_state.num_pressure_results;
        }

        milton_input.flags = (MiltonInputFlags)( input_flags | (int)milton_input.flags );

        mlt_assert (platform_state.num_point_results <= platform_state.num_pressure_results);

        milton_input.input_count = platform_state.num_point_results;

        v2i pan_delta = sub2i(platform_state.pan_point, platform_state.pan_start);
        if (pan_delta.x != 0 ||
            pan_delta.y != 0 ||
            platform_state.width != milton_state->view->screen_size.x ||
            platform_state.height != milton_state->view->screen_size.y)
        {
            b32 pan_ok = milton_resize_and_pan(milton_state, pan_delta, {platform_state.width, platform_state.height});
            if (!pan_ok)
            {
                // TODO: Turn panning off

                // Force a full re-render.
                // The hover point gets updated and the renderer does a memcpy on most of the screen.
                // TODO: Remove use of MiltonInputFlags_FULL_REFRESH after switching to HW rendering...
                input_flags |= MiltonInputFlags_FULL_REFRESH;
                milton_state->flags |= MiltonStateFlags_REQUEST_QUALITY_REDRAW;
            }
            else if (!platform_state.is_panning && !pan_ok)
            {
                INVALID_CODE_PATH;
            }
        }
        milton_input.pan_delta = pan_delta;

        // Reset pan_start. Delta is not cumulative.
        platform_state.pan_start = platform_state.pan_point;

        // ==== Update and render
        PROFILE_GRAPH_PUSH(polling);
        milton_update_and_render(milton_state, &milton_input);
        if ( !(milton_state->flags & MiltonStateFlags_RUNNING) )
        {
            platform_state.should_quit = true;
        }
        PROFILE_GRAPH_BEGIN(GL);
        ImGui::Render();
        PROFILE_GRAPH_PUSH(GL);
        PROFILE_GRAPH_BEGIN(system);
        SDL_GL_SwapWindow(window);

        // IMGUI events might update until the frame after they are created.
        if (!platform_state.force_next_frame)
        {
            SDL_WaitEvent(NULL);
        }
        else
        {
            platform_state.force_next_frame = true;
        }
    }

#if defined(_WIN32) || defined(__LINUX__)
    EasyTab_Unload();
#endif

    // Release pages. Not really necessary but we don't want to piss off leak
    // detectors, do we?
    platform_deallocate(big_chunk_of_memory);

    bool save_prefs = prefs.width != platform_state.width || prefs.height != platform_state.height;
    if (save_prefs)
    {
        prefs.width  = platform_state.width;
        prefs.height = platform_state.height;
        milton_prefs_save(&prefs);
    }

    SDL_Quit();

    return 0;
}


