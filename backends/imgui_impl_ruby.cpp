// dear imgui: Platform Backend for Ruby
// This needs to be used along with a Renderer (e.g. OpenGL3, Vulkan, WebGPU..)

#include "imgui.h"
#include "imgui_impl_ruby.h"

// Ruby
#include <Ruby/RubyWindow.h>
#include <Ruby/RubyMonitor.h>

enum class RubyClientAPI
{
	Unknown,
	OpenGL,
	Vulkan
};

class ImGui_ImplRuby_MainEventHandler;
struct ImGui_ImplRuby_Data
{
	RubyWindow*             Window;
	RubyClientAPI           ClientApi;
	double                  Time;
	RubyWindow*             MouseWindow;
	int						MouseCursor[ ImGuiMouseCursor_COUNT ];
	RubyWindow*             KeyOwnerWindows[512];
	bool                    InstalledCallbacks;
	bool                    WantUpdateMonitors;
    ImVec2                  LastValidMousePos;
	ImGui_ImplRuby_MainEventHandler* EventHandler;

	ImGui_ImplRuby_Data()   { memset(this, 0, sizeof(*this)); }
};

// Ruby Events

static bool ImGui_ImplRuby_DispatchEvent( RubyEvent& rEvent, RubyWindow* pWindow )
{
	switch( rEvent.Type )
	{
		case RubyEventType::MouseReleased:
		case RubyEventType::MousePressed:
		{
			RubyMouseEvent MouseEvent = ( RubyMouseEvent& ) rEvent;
			ImGui_ImplRuby_MouseButtonCallback( pWindow, MouseEvent.GetButton(), rEvent.Type == RubyEventType::MousePressed );
		} break;

		case RubyEventType::KeyPressed:
		{
			RubyKeyEvent KeyEvent = ( RubyKeyEvent& ) rEvent;
			ImGui_ImplRuby_KeyCallback( pWindow, KeyEvent.GetScancode(), true, KeyEvent.GetModifers() );
		} break;

		case RubyEventType::KeyReleased:
		{
			RubyKeyEvent KeyEvent = ( RubyKeyEvent& ) rEvent;
			ImGui_ImplRuby_KeyCallback( pWindow, KeyEvent.GetScancode(), false, 0 );
		} break;

		case RubyEventType::InputCharacter:
		{
			RubyCharacterEvent CharEvent = ( RubyCharacterEvent& ) rEvent;
			ImGui_ImplRuby_CharCallback( CharEvent.GetCharacter() );
		} break;

        case RubyEventType::MouseMoved:
        {
            RubyMouseMoveEvent MouseMoveEvent = ( RubyMouseMoveEvent& ) rEvent;
            ImGui_ImplRuby_CursorPosCallback( pWindow, MouseMoveEvent.GetX(), MouseMoveEvent.GetY() );
        } break;

		case RubyEventType::MouseEnterWindow:
		case RubyEventType::MouseLeaveWindow:
		{
			ImGui_ImplRuby_CursorEnterCallback( pWindow, rEvent.Type == RubyEventType::MouseEnterWindow );
		} break;
		
		case RubyEventType::WindowFocus: 
		{
			RubyFocusEvent FocusEvent = ( RubyFocusEvent& ) rEvent;

			ImGuiIO& io = ImGui::GetIO();
			io.AddFocusEvent( FocusEvent.GetState() );
		} break;

		case RubyEventType::MouseScroll:
		{
			RubyMouseScrollEvent ScrollEvent = ( RubyMouseScrollEvent& ) rEvent;
			ImGui_ImplRuby_ScrollCallback( ScrollEvent.GetOffsetX(), ScrollEvent.GetOffsetY() );
		} break;
	}

	return true;
}

// ImGui and Ruby event handler (only for the main window)
class ImGui_ImplRuby_MainEventHandler : public RubyEventTarget
{
public:
	ImGui_ImplRuby_MainEventHandler() {}

	~ImGui_ImplRuby_MainEventHandler() 
	{
		PrevUserEventTarget = nullptr;
		BackendData = nullptr;
	}

	bool OnEvent( RubyEvent& rEvent ) override
	{
		// Call the users event target first.
		bool handled = false;
		
		if( PrevUserEventTarget )
			handled = PrevUserEventTarget->OnEvent( rEvent );

		ImGui_ImplRuby_DispatchEvent( rEvent, BackendData->Window );
		
		return handled;
	}

public:
	RubyEventTarget* PrevUserEventTarget = nullptr;
	ImGui_ImplRuby_Data* BackendData = nullptr;
};

// ImGui and Ruby event handler (only for windows that we own, and only if we are using multi-viewports)
class ImGui_ImplRuby_EventHandler : public RubyEventTarget
{
public:
	ImGui_ImplRuby_EventHandler() {}
	ImGui_ImplRuby_EventHandler( ImGuiViewport* viewport ) : pViewport( viewport ) {}

	~ImGui_ImplRuby_EventHandler() {}

	bool OnEvent( RubyEvent& rEvent ) override
	{
		if( !pViewport )
			return false;

		ImGui_ImplRuby_DispatchEvent( rEvent, (RubyWindow*)pViewport->PlatformHandle );

		switch( rEvent.Type )
		{
			case RubyEventType::WindowMoved:
				pViewport->PlatformRequestMove = true;
				break;

			case RubyEventType::Resize:
				pViewport->PlatformRequestResize = true;
				break;

			case RubyEventType::Close:
				pViewport->PlatformRequestClose = true;
				return false;
		}

		return true;
	}

public:
	ImGuiViewport* pViewport = nullptr;
};


// Backend data stored in io.BackendPlatformUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
static ImGui_ImplRuby_Data* ImGui_ImplRuby_GetBackendData()
{
	return ImGui::GetCurrentContext() ? ( ImGui_ImplRuby_Data* )ImGui::GetIO().BackendPlatformUserData : NULL;
}

// Forward Declarations
static void ImGui_ImplRuby_UpdateMonitors();
static void ImGui_ImplRuby_InitPlatformInterface();
static void ImGui_ImplRuby_ShutdownPlatformInterface();

// Functions
static const char* ImGui_ImplRuby_GetClipboardText(void* user_data)
{
    ImGui_ImplRuby_Data* bd = ImGui_ImplRuby_GetBackendData();

    const char* text = bd->Window->GetClipboardText();
	return text;
}

static void ImGui_ImplRuby_SetClipboardText(void* user_data, const char* text)
{
    ImGui_ImplRuby_Data* bd = ImGui_ImplRuby_GetBackendData();

    bd->Window->SetClipboardText( text );
}

static ImGuiKey ImGui_ImplRuby_KeyToImGuiKey( int scancode )
{
    switch( scancode )
    {
        case A: return ImGuiKey_A;
        case B: return ImGuiKey_B;
        case C: return ImGuiKey_C;
        case D: return ImGuiKey_D;
        case E: return ImGuiKey_E;
        case F: return ImGuiKey_F;
        case G: return ImGuiKey_G;
        case H: return ImGuiKey_H;
        case I: return ImGuiKey_I;
        case J: return ImGuiKey_J;
        case K: return ImGuiKey_K;
        case L: return ImGuiKey_L;
        case M: return ImGuiKey_M;
        case N: return ImGuiKey_N;
        case O: return ImGuiKey_O;
        case P: return ImGuiKey_P;
        case Q: return ImGuiKey_Q;
        case R: return ImGuiKey_R;
        case S: return ImGuiKey_S;
        case T: return ImGuiKey_T;
        case U: return ImGuiKey_U;
        case V: return ImGuiKey_V;
        case W: return ImGuiKey_W;
        case X: return ImGuiKey_X;
        case Y: return ImGuiKey_Y;
        case Z: return ImGuiKey_Z;
        case Num0: return ImGuiKey_0;
        case Num1: return ImGuiKey_1;
        case Num2: return ImGuiKey_2;
        case Num3: return ImGuiKey_3;
        case Num4: return ImGuiKey_4;
        case Num5: return ImGuiKey_5;
        case Num6: return ImGuiKey_6;
        case Num7: return ImGuiKey_7;
        case Num8: return ImGuiKey_8;
        case Num9: return ImGuiKey_9;
        case Space: return ImGuiKey_Space;
        case Enter: return ImGuiKey_Enter;
        case Tab: return ImGuiKey_Tab;
        case Esc: return ImGuiKey_Escape;
        case Backspace: return ImGuiKey_Backspace;
        case CapsLock: return ImGuiKey_CapsLock;
        case Shift: return ImGuiKey_LeftShift;
        case Ctrl: return ImGuiKey_LeftCtrl;
        case Alt: return ImGuiKey_LeftAlt;
        case OSKey: return ImGuiKey_LeftSuper;
        case Insert: return ImGuiKey_Insert;
        case Delete: return ImGuiKey_Delete;
        case Home: return ImGuiKey_Home;
        case End: return ImGuiKey_End;
        case PageUp: return ImGuiKey_PageUp;
        case PageDown: return ImGuiKey_PageDown;
        case Numpad0: return ImGuiKey_Keypad0;
        case Numpad1: return ImGuiKey_Keypad1;
        case Numpad2: return ImGuiKey_Keypad2;
        case Numpad3: return ImGuiKey_Keypad3;
        case Numpad4: return ImGuiKey_Keypad4;
        case Numpad5: return ImGuiKey_Keypad5;
        case Numpad6: return ImGuiKey_Keypad6;
        case Numpad7: return ImGuiKey_Keypad7;
        case Numpad8: return ImGuiKey_Keypad8;
        case Numpad9: return ImGuiKey_Keypad9;
        case NumpadAdd: return ImGuiKey_KeypadAdd;
        case NumpadSubtract: return ImGuiKey_KeypadSubtract;
        case NumpadMultiply: return ImGuiKey_KeypadMultiply;
        case NumpadDivide: return ImGuiKey_KeypadDivide;
        case LeftArrow: return ImGuiKey_LeftArrow;
        case UpArrow: return ImGuiKey_UpArrow;
        case RightArrow: return ImGuiKey_RightShift;
        case DownArrow: return ImGuiKey_DownArrow;
        case F1: return ImGuiKey_F1;
        case F2: return ImGuiKey_F2;
        case F3: return ImGuiKey_F3;
        case F4: return ImGuiKey_F4;
        case F5: return ImGuiKey_F5;
        case F6: return ImGuiKey_F6;
        case F7: return ImGuiKey_F7;
        case F8: return ImGuiKey_F8;
        case F9: return ImGuiKey_F9;
        case F10: return ImGuiKey_F10;
        case F11: return ImGuiKey_F11;
        case F12: return ImGuiKey_F12;
        case RightCtrl: return ImGuiKey_RightCtrl;
        case RightShift: return ImGuiKey_RightShift;
        case RightAlt: return ImGuiKey_RightAlt;

        case UnknownKey:
        default: return ImGuiKey_None;
    }
}

void ImGui_ImplRuby_MouseButtonCallback(RubyWindow* window, int button, bool state)
{
	ImGuiIO& io = ImGui::GetIO();

	if( button >= 0 && button < ImGuiMouseButton_COUNT )
		io.AddMouseButtonEvent( button, state );
}

void ImGui_ImplRuby_ScrollCallback( double xoffset, double yoffset)
{
	ImGuiIO& io = ImGui::GetIO();
	io.AddMouseWheelEvent( (float)xoffset, (float)yoffset );
}

static void ImGui_ImplRuby_UpdateKeyModifiers( RubyWindow* window )
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent( ImGuiMod_Ctrl,
        ( window->IsKeyDown( RubyKey::Ctrl ) || window->IsKeyDown( RubyKey::RightCtrl ) ) );
    io.AddKeyEvent( ImGuiMod_Alt,
        ( window->IsKeyDown( RubyKey::Alt ) || window->IsKeyDown( RubyKey::RightAlt ) ) );
    io.AddKeyEvent( ImGuiMod_Shift,
        ( window->IsKeyDown( RubyKey::Shift ) || window->IsKeyDown( RubyKey::RightShift ) ) );
    io.AddKeyEvent( ImGuiMod_Super,
        ( window->IsKeyDown( RubyKey::OSKey ) ) );
}

void ImGui_ImplRuby_KeyCallback( RubyWindow* window, int scancode, bool state, int mods)
{
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplRuby_Data* bd = ImGui_ImplRuby_GetBackendData();

    ImGui_ImplRuby_UpdateKeyModifiers( window );

    ImGuiKey imgui_key = ImGui_ImplRuby_KeyToImGuiKey( scancode );
    io.AddKeyEvent( imgui_key, state );
    io.SetKeyEventNativeData( imgui_key, scancode, scancode );
}

void ImGui_ImplRuby_MouseHoverWindowCallback( bool state )
{
	if( !state )
		return;

	ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
	ImGuiIO& io = ImGui::GetIO();

	for( int n = 0; n < platform_io.Viewports.Size; n++ )
	{
		ImGuiViewport* viewport = platform_io.Viewports[ n ];
		io.MouseHoveredViewport = viewport->ID;
	}
}

void ImGui_ImplRuby_WindowFocusCallback( RubyWindow* window, bool focused )
{
	ImGuiIO& io = ImGui::GetIO();
	io.AddFocusEvent(focused != 0);
}

void ImGui_ImplRuby_CursorEnterCallback( RubyWindow* window, bool entered )
{
    ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplRuby_Data* bd = ImGui_ImplRuby_GetBackendData();

    if( entered )
    {
        bd->MouseWindow = window;
        io.AddMousePosEvent( bd->LastValidMousePos.x, bd->LastValidMousePos.y );
    }
    else if( !entered && bd->MouseWindow == window )
    {
        bd->LastValidMousePos = io.MousePos;
        bd->MouseWindow = nullptr;
        io.AddMousePosEvent( -FLT_MAX, -FLT_MAX );
    }
}

void ImGui_ImplRuby_CursorPosCallback( RubyWindow* window, float x, float y )
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplRuby_Data* bd = ImGui_ImplRuby_GetBackendData();

    if( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
    {
        RubyIVec2 pos = window->GetPosition();
        x += pos.x;
        y += pos.y;
    }

    io.AddMousePosEvent( x, y );
    bd->LastValidMousePos = ImVec2( x, y );
}

void ImGui_ImplRuby_CharCallback( unsigned int c )
{
	ImGuiIO& io = ImGui::GetIO();
	io.AddInputCharacter( c );
}

static bool ImGui_ImplRuby_Init( RubyWindow* window, bool install_callbacks, RubyClientAPI client_api)
{
	ImGuiIO& io = ImGui::GetIO();
	IM_ASSERT(io.BackendPlatformUserData == NULL && "Already initialized a platform backend!");

	// Setup backend capabilities flags
	ImGui_ImplRuby_Data* bd = IM_NEW(ImGui_ImplRuby_Data)();
	io.BackendPlatformUserData = (void*)bd;
	io.BackendPlatformName = "imgui_impl_ruby";
	io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
	io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)
	io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;    // We can create multi-viewports on the Platform side (optional)
#if defined(_WIN32)
	io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport; // We can set io.MouseHoveredViewport correctly (optional, not easy)
#endif

	bd->Window = window;
	bd->Time = 0.0;
	bd->WantUpdateMonitors = true;

	bd->EventHandler = IM_NEW( ImGui_ImplRuby_MainEventHandler )();
	bd->EventHandler->BackendData = bd;
	bd->EventHandler->PrevUserEventTarget = bd->Window->GetEventTarget();

	bd->Window->SetEventTarget( bd->EventHandler );

	io.SetClipboardTextFn = ImGui_ImplRuby_SetClipboardText;
	io.GetClipboardTextFn = ImGui_ImplRuby_GetClipboardText;
	io.ClipboardUserData = bd->Window;

	bd->MouseCursor[ ImGuiMouseCursor_Arrow ]      = (int)RubyCursorType::Arrow;
	bd->MouseCursor[ ImGuiMouseCursor_Hand ]       = (int)RubyCursorType::Hand;
	bd->MouseCursor[ ImGuiMouseCursor_TextInput ]  = (int)RubyCursorType::IBeam;
	bd->MouseCursor[ ImGuiMouseCursor_NotAllowed ] = (int)RubyCursorType::NotAllowed;
	bd->MouseCursor[ ImGuiMouseCursor_ResizeNS ]   = (int)RubyCursorType::ResizeNS;
	bd->MouseCursor[ ImGuiMouseCursor_ResizeEW ]   = (int)RubyCursorType::ResizeEW;
	bd->MouseCursor[ ImGuiMouseCursor_ResizeNWSE ] = (int)RubyCursorType::ResizeNWSE;
	bd->MouseCursor[ ImGuiMouseCursor_ResizeNESW ] = (int)RubyCursorType::ResizeNESW;

	ImGui_ImplRuby_UpdateMonitors();

	// Our mouse update function expect PlatformHandle to be filled for the main viewport
	ImGuiViewport* main_viewport = ImGui::GetMainViewport();
	main_viewport->PlatformHandle = (void*)bd->Window;
#ifdef _WIN32
	main_viewport->PlatformHandleRaw = (HWND)bd->Window->GetNativeHandle();
#endif
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		ImGui_ImplRuby_InitPlatformInterface();

	bd->ClientApi = client_api;
	return true;
}

bool ImGui_ImplRuby_InitForOpenGL( RubyWindow* window )
{
	return ImGui_ImplRuby_Init(window, false, RubyClientAPI::OpenGL );
}

bool ImGui_ImplRuby_InitForVulkan( RubyWindow* window )
{
	return ImGui_ImplRuby_Init(window, false, RubyClientAPI::Vulkan );
}

bool ImGui_ImplRuby_InitForOther( RubyWindow* window )
{
	return ImGui_ImplRuby_Init(window, false, RubyClientAPI::Unknown );
}

void ImGui_ImplRuby_Shutdown()
{
	ImGui_ImplRuby_Data* bd = ImGui_ImplRuby_GetBackendData();
	IM_ASSERT(bd != nullptr && "No platform backend to shutdown, or already shutdown?");
	ImGuiIO& io = ImGui::GetIO();

	ImGui_ImplRuby_ShutdownPlatformInterface();

	bd->Window->SetEventTarget( bd->EventHandler->PrevUserEventTarget );
	delete bd->EventHandler;

	io.BackendPlatformName = nullptr;
	io.BackendPlatformUserData = nullptr;
	IM_DELETE(bd);
}

static void ImGui_ImplRuby_UpdateMouseData()
{
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplRuby_Data* bd = ImGui_ImplRuby_GetBackendData();
	ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

    ImGuiID mouse_viewport_id = 0;
    const ImVec2 mouse_pos_prev = io.MousePos;

	for( int n = 0; n < platform_io.Viewports.Size; n++ )
	{
		ImGuiViewport* viewport = platform_io.Viewports[ n ];
		RubyWindow* window = ( RubyWindow* ) viewport->PlatformHandle;

		if( window->IsFocused() )
		{
			if( io.WantSetMousePos )
			{
				window->SetMousePos( ( double ) mouse_pos_prev.x - viewport->Pos.x, ( double ) mouse_pos_prev.y - viewport->Pos.y );
			}

			if( !bd->MouseWindow )
			{
				double x, y;
				window->GetMousePos( &x, &y );

				if( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
				{
					RubyIVec2 pos = window->GetPosition();

                    x += pos.x;
                    y += pos.y;
				}

                bd->LastValidMousePos = ImVec2( static_cast< float >( x ), static_cast< float >( y ) );
				io.AddMousePosEvent( static_cast< float >( x ), static_cast< float >( y ) );
			}
		}

        const bool window_no_input = ( viewport->Flags & ImGuiViewportFlags_NoInputs ) != 0;
		if( window->MouseInWindow() && !window_no_input )
		{
			mouse_viewport_id = viewport->ID;
		}
	}

	if( io.BackendFlags & ImGuiBackendFlags_HasMouseHoveredViewport )
		io.AddMouseViewportEvent( mouse_viewport_id );
}

static void ImGui_ImplRuby_UpdateMouseCursor()
{
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplRuby_Data* bd = ImGui_ImplRuby_GetBackendData();

	if( ( io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange ) || bd->Window->GetCursorMode() == RubyCursorMode::Locked )
		return;

	ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
	ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

	for( int n = 0; n < platform_io.Viewports.Size; n++ )
	{
		RubyWindow* window = ( RubyWindow* ) platform_io.Viewports[ n ]->PlatformHandle;

		if( imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor )
		{
			// Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
			window->SetMouseCursorMode( RubyCursorMode::Hidden );
		}
		else
		{
			window->SetMouseCursor( ( RubyCursorType )bd->MouseCursor[ imgui_cursor ] );
            window->SetMouseCursorMode( RubyCursorMode::Normal );
		}
	}
}

static void ImGui_ImplRuby_UpdateGamepads()
{
}

static void ImGui_ImplRuby_UpdateMonitors()
{
	ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
	ImGui_ImplRuby_Data* bd = ImGui_ImplRuby_GetBackendData();

	std::vector<RubyMonitor> monitors = RubyGetAllMonitors();

	for( const auto& rMonitor : monitors )
	{
		ImGuiPlatformMonitor monitor;

		monitor.MainSize = ImVec2( ( float ) rMonitor.MonitorSize.x, ( float ) rMonitor.MonitorSize.x );
		monitor.WorkSize = ImVec2( ( float ) rMonitor.WorkSize.x, ( float ) rMonitor.WorkSize.y );

		monitor.WorkPos = ImVec2( ( float ) rMonitor.MonitorPosition.x, ( float ) rMonitor.MonitorPosition.y );
		monitor.MainPos = monitor.WorkPos;

		platform_io.Monitors.push_back( monitor );
	}
	bd->WantUpdateMonitors = false;
}

void ImGui_ImplRuby_NewFrame()
{
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplRuby_Data* bd = ImGui_ImplRuby_GetBackendData();
	IM_ASSERT(bd != NULL && "Did you call ImGui_ImplRuby_InitForXXX()?");

	// Setup display size (every frame to accommodate for window resizing)
	int w, h;
	int display_w, display_h;

	w = bd->Window->GetWidth();
	h = bd->Window->GetHeight();
	display_w = w;
	display_h = h;

	io.DisplaySize = ImVec2( (float)w, (float)h );
   
	if (w > 0 && h > 0)
		io.DisplayFramebufferScale = ImVec2((float)display_w / w, (float)display_h / h);
	
	if (bd->WantUpdateMonitors)
		ImGui_ImplRuby_UpdateMonitors();

	// Setup time step
	double current_time = bd->Window->GetTime();
	io.DeltaTime = bd->Time > 0.0 ? (float)(current_time - bd->Time) : (float)(1.0f / 60.0f);
	bd->Time = current_time;

	ImGui_ImplRuby_UpdateEvents();

	ImGui_ImplRuby_UpdateMouseData();
	ImGui_ImplRuby_UpdateMouseCursor();

	// Update game controllers (if enabled and available)
	ImGui_ImplRuby_UpdateGamepads();
}

#pragma region MultiViewport

//--------------------------------------------------------------------------------------------------------
// MULTI-VIEWPORT / PLATFORM INTERFACE SUPPORT
// This is an _advanced_ and _optional_ feature, allowing the backend to create and handle multiple viewports simultaneously.
// If you are new to dear imgui or creating a new binding for dear imgui, it is recommended that you completely ignore this section first..
//--------------------------------------------------------------------------------------------------------

// Helper structure we store in the void* RenderUserData field of each ImGuiViewport to easily retrieve our backend data.
struct ImGui_ImplRuby_ViewportData
{
	RubyWindow* Window;
	bool        WindowOwned;

	ImGui_ImplRuby_ViewportData()  { Window = NULL; WindowOwned = false; }
	~ImGui_ImplRuby_ViewportData() { IM_ASSERT(Window == NULL); }
};

static void ImGui_ImplRuby_WindowCloseCallback( RubyWindow* window )
{
	if (ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle(window))
		viewport->PlatformRequestClose = true;
}

static void ImGui_ImplRuby_CreateWindow(ImGuiViewport* viewport)
{
	ImGui_ImplRuby_Data* bd = ImGui_ImplRuby_GetBackendData();
	ImGui_ImplRuby_ViewportData* vd = IM_NEW( ImGui_ImplRuby_ViewportData )( );
	viewport->PlatformUserData = vd;

	RubyWindowSpecification spec;
	spec.Style = ( viewport->Flags & ImGuiViewportFlags_NoDecoration ) ? RubyStyle::Borderless : RubyStyle::Default;
	spec.GraphicsAPI = (RubyGraphicsAPI) bd->ClientApi;
	spec.Name = "No Name Yet.";
	spec.ShowNow = false;
	spec.Width = ( uint32_t ) viewport->Size.x;
	spec.Height = ( uint32_t ) viewport->Size.y;

	vd->Window = new RubyWindow( spec );
	vd->Window->SetEventTarget( IM_NEW( ImGui_ImplRuby_EventHandler )( viewport ) );
	vd->WindowOwned = true;

	viewport->PlatformHandleRaw = vd->Window->GetNativeHandle();

#ifdef _WIN32
	viewport->PlatformHandle = (void*)vd->Window;
#endif

	vd->Window->SetPosition( ( int ) viewport->Pos.x, ( int ) viewport->Pos.y );
}

static void ImGui_ImplRuby_DestroyWindow(ImGuiViewport* viewport)
{
	ImGui_ImplRuby_Data* bd = ImGui_ImplRuby_GetBackendData();

	if( ImGui_ImplRuby_ViewportData* vd = ( ImGui_ImplRuby_ViewportData* ) viewport->PlatformUserData )
	{
		if( vd->WindowOwned )
		{
		   delete vd->Window;
		}

		vd->Window = nullptr;
		IM_DELETE( vd );
	}
	viewport->PlatformUserData = viewport->PlatformHandle = nullptr;
}

static void ImGui_ImplRuby_ShowWindow(ImGuiViewport* viewport)
{
	ImGui_ImplRuby_ViewportData* vd = ( ImGui_ImplRuby_ViewportData* ) viewport->PlatformUserData;

	// Temp: Ruby does not support not showing a icon on the task bar so we will ask win32 to do it for us
#if defined( _WIN32 )
	HWND hwnd = ( HWND ) viewport->PlatformHandleRaw;

	if( viewport->Flags & ImGuiViewportFlags_NoTaskBarIcon )
	{
		LONG Style = ::GetWindowLong( hwnd, GWL_EXSTYLE );
		Style &= ~( WS_EX_APPWINDOW );
		Style |= WS_EX_TOOLWINDOW;

		::SetWindowLong( hwnd, GWL_EXSTYLE, Style );
	}

#endif

	vd->Window->Show();
}

void ImGui_ImplRuby_UpdateEvents()
{
	ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

	for( int n = 0; n < platform_io.Viewports.Size; n++ )
	{
		ImGuiViewport* viewport = platform_io.Viewports[ n ];
		ImGui_ImplRuby_ViewportData* vd = ( ImGui_ImplRuby_ViewportData* ) viewport->PlatformUserData;

		RubyWindow* window = ( RubyWindow* ) vd->Window;

		if( window && vd->WindowOwned )
			window->PollEvents();
	}
}

static ImVec2 ImGui_ImplRuby_GetWindowPos(ImGuiViewport* viewport)
{
	ImGui_ImplRuby_ViewportData* vd = ( ImGui_ImplRuby_ViewportData* ) viewport->PlatformUserData;

	return { ( float ) vd->Window->GetPosition().x, ( float ) vd->Window->GetPosition().y };
}

static void ImGui_ImplRuby_SetWindowPos(ImGuiViewport* viewport, ImVec2 pos)
{
	ImGui_ImplRuby_ViewportData* vd = ( ImGui_ImplRuby_ViewportData* ) viewport->PlatformUserData;

	vd->Window->SetPosition( ( int ) pos.x, ( int ) pos.y );
}

static ImVec2 ImGui_ImplRuby_GetWindowSize(ImGuiViewport* viewport)
{
	ImGui_ImplRuby_ViewportData* vd = ( ImGui_ImplRuby_ViewportData* ) viewport->PlatformUserData;

	return { ( float ) vd->Window->GetWidth(), ( float ) vd->Window->GetHeight() };
}

static void ImGui_ImplRuby_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
	ImGui_ImplRuby_ViewportData* vd = ( ImGui_ImplRuby_ViewportData* ) viewport->PlatformUserData;

	vd->Window->Resize( ( uint32_t ) size.x, ( uint32_t ) size.y );
}

static void ImGui_ImplRuby_SetWindowTitle(ImGuiViewport* viewport, const char* title)
{
	ImGui_ImplRuby_ViewportData* vd = (ImGui_ImplRuby_ViewportData*)viewport->PlatformUserData;

   vd->Window->ChangeTitle( title );
}

static void ImGui_ImplRuby_SetWindowFocus(ImGuiViewport* viewport)
{
	ImGui_ImplRuby_ViewportData* vd = (ImGui_ImplRuby_ViewportData*)viewport->PlatformUserData;

	vd->Window->Focus();
}

static bool ImGui_ImplRuby_GetWindowFocus(ImGuiViewport* viewport)
{
	ImGui_ImplRuby_ViewportData* vd = ( ImGui_ImplRuby_ViewportData* ) viewport->PlatformUserData;

	return vd->Window->IsFocused();
}

static bool ImGui_ImplRuby_GetWindowMinimized(ImGuiViewport* viewport)
{
	ImGui_ImplRuby_ViewportData* vd = ( ImGui_ImplRuby_ViewportData* ) viewport->PlatformUserData;

	return vd->Window->Minimized();
}

static void ImGui_ImplRuby_RenderWindow(ImGuiViewport* viewport, void*)
{
}

static void ImGui_ImplRuby_SwapBuffers(ImGuiViewport* viewport, void*)
{
	ImGui_ImplRuby_ViewportData* vd = ( ImGui_ImplRuby_ViewportData* ) viewport->PlatformUserData;

	vd->Window->GLSwapBuffers();
}

//--------------------------------------------------------------------------------------------------------
// IME (Input Method Editor) basic support for e.g. Asian language users
//--------------------------------------------------------------------------------------------------------

// We provide a Win32 implementation because this is such a common issue for IME users
#if defined(_WIN32) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS) && !defined(IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS)
#define HAS_WIN32_IME   1
#include <imm.h>
#ifdef _MSC_VER
#pragma comment(lib, "imm32")
#endif
static void ImGui_ImplWin32_SetImeInputPos(ImGuiViewport* viewport, ImVec2 pos)
{
	COMPOSITIONFORM cf = { CFS_FORCE_POSITION, { (LONG)(pos.x - viewport->Pos.x), (LONG)(pos.y - viewport->Pos.y) }, { 0, 0, 0, 0 } };
	if (HWND hwnd = (HWND)viewport->PlatformHandleRaw)
		if (HIMC himc = ::ImmGetContext(hwnd))
		{
			::ImmSetCompositionWindow(himc, &cf);
			::ImmReleaseContext(hwnd, himc);
		}
}
#else
#define HAS_WIN32_IME   0
#endif

//--------------------------------------------------------------------------------------------------------
// Vulkan support (the Vulkan renderer needs to call a platform-side support function to create the surface)
//--------------------------------------------------------------------------------------------------------

#ifndef VULKAN_H_
#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
#if defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(_M_X64) || defined(__ia64) || defined (_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef struct object##_T *object;
#else
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef uint64_t object;
#endif
VK_DEFINE_HANDLE(VkInstance)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkSurfaceKHR)
struct VkAllocationCallbacks;
enum VkResult { VK_RESULT_MAX_ENUM = 0x7FFFFFFF };
#endif // VULKAN_H_

static int ImGui_ImplRuby_CreateVkSurface(ImGuiViewport* viewport, ImU64 vk_instance, const void* vk_allocator, ImU64* out_vk_surface)
{
	ImGui_ImplRuby_Data* bd = ImGui_ImplRuby_GetBackendData();
	ImGui_ImplRuby_ViewportData* vd = (ImGui_ImplRuby_ViewportData*)viewport->PlatformUserData;

	IM_UNUSED(bd);
	IM_ASSERT(bd->ClientApi == RubyClientAPI::Vulkan);

	VkResult err = vd->Window->CreateVulkanWindowSurface( ( VkInstance ) vk_instance, ( VkSurfaceKHR* ) out_vk_surface );

	return (int)err;
}

static void ImGui_ImplRuby_InitPlatformInterface()
{
	// Register platform interface (will be coupled with a renderer interface)
	ImGui_ImplRuby_Data* bd = ImGui_ImplRuby_GetBackendData();
	ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
	platform_io.Platform_CreateWindow = ImGui_ImplRuby_CreateWindow;
	platform_io.Platform_DestroyWindow = ImGui_ImplRuby_DestroyWindow;
	platform_io.Platform_ShowWindow = ImGui_ImplRuby_ShowWindow;
	platform_io.Platform_SetWindowPos = ImGui_ImplRuby_SetWindowPos;
	platform_io.Platform_GetWindowPos = ImGui_ImplRuby_GetWindowPos;
	platform_io.Platform_SetWindowSize = ImGui_ImplRuby_SetWindowSize;
	platform_io.Platform_GetWindowSize = ImGui_ImplRuby_GetWindowSize;
	platform_io.Platform_SetWindowFocus = ImGui_ImplRuby_SetWindowFocus;
	platform_io.Platform_GetWindowFocus = ImGui_ImplRuby_GetWindowFocus;
	platform_io.Platform_GetWindowMinimized = ImGui_ImplRuby_GetWindowMinimized;
	platform_io.Platform_SetWindowTitle = ImGui_ImplRuby_SetWindowTitle;
	platform_io.Platform_RenderWindow = ImGui_ImplRuby_RenderWindow;
	platform_io.Platform_SwapBuffers = ImGui_ImplRuby_SwapBuffers;
	platform_io.Platform_CreateVkSurface = ImGui_ImplRuby_CreateVkSurface;

	// Register main window handle (which is owned by the main application, not by us)
	// This is mostly for simplicity and consistency, so that our code (e.g. mouse handling etc.) can use same logic for main and secondary viewports.
	ImGuiViewport* main_viewport = ImGui::GetMainViewport();
	ImGui_ImplRuby_ViewportData* vd = IM_NEW(ImGui_ImplRuby_ViewportData)();
	vd->Window = bd->Window;
	vd->WindowOwned = false;
	main_viewport->PlatformUserData = vd;
	main_viewport->PlatformHandle = (void*)bd->Window;
}

static void ImGui_ImplRuby_ShutdownPlatformInterface()
{
	ImGui::DestroyPlatformWindows();
}

#pragma endregion
