#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#ifdef _MSC_VER
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#endif
#endif

#include "moonbit.h"

typedef struct moonbit_tray_state {
#ifdef _WIN32
  HWND hwnd;
  NOTIFYICONDATAW icon_data;
#endif
  int32_t visible;
  char last_error[256];
} moonbit_tray_state_t;

static char moonbit_tray_create_error[256];

static moonbit_tray_state_t *moonbit_tray_from_handle(int64_t handle) {
  return (moonbit_tray_state_t *)(uintptr_t)handle;
}

static int64_t moonbit_tray_to_handle(moonbit_tray_state_t *state) {
  return (int64_t)(uintptr_t)state;
}

static void moonbit_tray_set_message(char *buffer, size_t size, const char *message) {
  if (size == 0) {
    return;
  }
  if (message == NULL) {
    buffer[0] = '\0';
    return;
  }
  snprintf(buffer, size, "%s", message);
}

static moonbit_bytes_t moonbit_tray_copy_message(const char *message) {
  int32_t len;
  moonbit_bytes_t bytes;
  if (message == NULL) {
    message = "";
  }
  len = (int32_t)strlen(message);
  bytes = moonbit_make_bytes(len, 0);
  if (len > 0) {
    memcpy(bytes, message, (size_t)len);
  }
  return bytes;
}

#ifdef _WIN32
static ATOM moonbit_tray_window_class = 0;

static LRESULT CALLBACK moonbit_tray_window_proc(
    HWND hwnd,
    UINT message,
    WPARAM w_param,
    LPARAM l_param) {
  (void)w_param;
  (void)l_param;
  switch (message) {
  case WM_CLOSE:
    DestroyWindow(hwnd);
    return 0;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  default:
    return DefWindowProcW(hwnd, message, w_param, l_param);
  }
}

static int32_t moonbit_tray_ensure_window_class(void) {
  WNDCLASSEXW wc;
  if (moonbit_tray_window_class != 0) {
    return 1;
  }
  memset(&wc, 0, sizeof(wc));
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = moonbit_tray_window_proc;
  wc.hInstance = GetModuleHandleW(NULL);
  wc.lpszClassName = L"MoonBitTrayWindow";
  moonbit_tray_window_class = RegisterClassExW(&wc);
  return moonbit_tray_window_class != 0;
}

static wchar_t *moonbit_tray_utf8_to_wide(moonbit_bytes_t value) {
  const char *text = (const char *)value;
  int32_t units;
  wchar_t *wide;
  if (text == NULL || text[0] == '\0') {
    return NULL;
  }
  units = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
  if (units <= 0) {
    return NULL;
  }
  wide = (wchar_t *)calloc((size_t)units, sizeof(wchar_t));
  if (wide == NULL) {
    return NULL;
  }
  if (MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, units) <= 0) {
    free(wide);
    return NULL;
  }
  return wide;
}

static void moonbit_tray_copy_tooltip(
    moonbit_tray_state_t *state,
    moonbit_bytes_t tooltip) {
  wchar_t *wide = moonbit_tray_utf8_to_wide(tooltip);
  if (wide == NULL) {
    state->icon_data.szTip[0] = L'\0';
    return;
  }
  wcsncpy(state->icon_data.szTip, wide, 127);
  state->icon_data.szTip[127] = L'\0';
  free(wide);
}

static HICON moonbit_tray_load_icon(moonbit_bytes_t icon) {
  wchar_t *path = moonbit_tray_utf8_to_wide(icon);
  HICON loaded = NULL;
  if (path != NULL) {
    loaded = (HICON)LoadImageW(
        NULL,
        path,
        IMAGE_ICON,
        0,
        0,
        LR_DEFAULTSIZE | LR_LOADFROMFILE);
    if (loaded == NULL) {
      ExtractIconExW(path, 0, NULL, &loaded, 1);
    }
    free(path);
  }
  if (loaded == NULL) {
    loaded = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
  }
  return loaded;
}

static int32_t moonbit_tray_replace_icon(
    moonbit_tray_state_t *state,
    moonbit_bytes_t icon) {
  HICON next_icon = moonbit_tray_load_icon(icon);
  if (next_icon == NULL) {
    moonbit_tray_set_message(state->last_error, sizeof(state->last_error), "failed to load tray icon");
    return 0;
  }
  if (state->icon_data.hIcon != NULL && state->icon_data.hIcon != next_icon) {
    DestroyIcon(state->icon_data.hIcon);
  }
  state->icon_data.hIcon = next_icon;
  state->icon_data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  return 1;
}
#endif

MOONBIT_FFI_EXPORT int32_t moonbit_tray_current_platform(void) {
#ifdef _WIN32
  return 1;
#elif defined(__linux__)
  return 2;
#elif defined(__APPLE__)
  return 3;
#else
  return 0;
#endif
}

MOONBIT_FFI_EXPORT int32_t moonbit_tray_is_supported(void) {
#ifdef _WIN32
  return 1;
#else
  return 0;
#endif
}

MOONBIT_FFI_EXPORT moonbit_bytes_t moonbit_tray_support_error(void) {
#ifdef _WIN32
  return moonbit_tray_copy_message("");
#elif defined(__linux__)
  return moonbit_tray_copy_message(
      "tray support is not available yet on Linux in this revision");
#elif defined(__APPLE__)
  return moonbit_tray_copy_message(
      "tray support is not available yet on macOS in this revision");
#else
  return moonbit_tray_copy_message(
      "tray support is not available on this operating system");
#endif
}

MOONBIT_FFI_EXPORT int64_t moonbit_tray_create(
    moonbit_bytes_t identifier,
    moonbit_bytes_t icon,
    moonbit_bytes_t tooltip) {
  moonbit_tray_state_t *state;
  (void)identifier;
#ifdef _WIN32
  if (!moonbit_tray_ensure_window_class()) {
    moonbit_tray_set_message(
        moonbit_tray_create_error,
        sizeof(moonbit_tray_create_error),
        "failed to register the tray window class");
    return 0;
  }
  state = (moonbit_tray_state_t *)calloc(1, sizeof(moonbit_tray_state_t));
  if (state == NULL) {
    moonbit_tray_set_message(
        moonbit_tray_create_error,
        sizeof(moonbit_tray_create_error),
        "failed to allocate tray state");
    return 0;
  }
  state->hwnd = CreateWindowExW(
      0,
      L"MoonBitTrayWindow",
      L"MoonBitTrayWindow",
      0,
      0,
      0,
      0,
      0,
      HWND_MESSAGE,
      NULL,
      GetModuleHandleW(NULL),
      NULL);
  if (state->hwnd == NULL) {
    moonbit_tray_set_message(
        moonbit_tray_create_error,
        sizeof(moonbit_tray_create_error),
        "failed to create the hidden tray window");
    free(state);
    return 0;
  }
  memset(&state->icon_data, 0, sizeof(state->icon_data));
  state->icon_data.cbSize = sizeof(state->icon_data);
  state->icon_data.hWnd = state->hwnd;
  state->icon_data.uID = 0x6D42;
  state->icon_data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  state->icon_data.uCallbackMessage = WM_APP + 1;
  moonbit_tray_copy_tooltip(state, tooltip);
  if (!moonbit_tray_replace_icon(state, icon)) {
    moonbit_tray_set_message(
        moonbit_tray_create_error,
        sizeof(moonbit_tray_create_error),
        state->last_error);
    DestroyWindow(state->hwnd);
    free(state);
    return 0;
  }
  moonbit_tray_set_message(state->last_error, sizeof(state->last_error), "");
  moonbit_tray_set_message(moonbit_tray_create_error, sizeof(moonbit_tray_create_error), "");
  return moonbit_tray_to_handle(state);
#else
  (void)icon;
  (void)tooltip;
  moonbit_tray_set_message(
      moonbit_tray_create_error,
      sizeof(moonbit_tray_create_error),
      "native tray backend is unavailable on this platform in this revision");
  return 0;
#endif
}

MOONBIT_FFI_EXPORT moonbit_bytes_t moonbit_tray_last_create_error(void) {
  return moonbit_tray_copy_message(moonbit_tray_create_error);
}

MOONBIT_FFI_EXPORT void moonbit_tray_destroy(int64_t handle) {
  moonbit_tray_state_t *state = moonbit_tray_from_handle(handle);
  if (state == NULL) {
    return;
  }
#ifdef _WIN32
  if (state->visible) {
    Shell_NotifyIconW(NIM_DELETE, &state->icon_data);
  }
  if (state->icon_data.hIcon != NULL) {
    DestroyIcon(state->icon_data.hIcon);
  }
  if (state->hwnd != NULL) {
    DestroyWindow(state->hwnd);
  }
#endif
  free(state);
}

MOONBIT_FFI_EXPORT int32_t moonbit_tray_show(
    int64_t handle,
    moonbit_bytes_t tooltip) {
  moonbit_tray_state_t *state = moonbit_tray_from_handle(handle);
#ifdef _WIN32
  if (state == NULL) {
    return 0;
  }
  moonbit_tray_copy_tooltip(state, tooltip);
  if (state->visible) {
    if (!Shell_NotifyIconW(NIM_MODIFY, &state->icon_data)) {
      moonbit_tray_set_message(state->last_error, sizeof(state->last_error), "Shell_NotifyIconW(NIM_MODIFY) failed");
      return 0;
    }
    return 1;
  }
  if (!Shell_NotifyIconW(NIM_ADD, &state->icon_data)) {
    moonbit_tray_set_message(state->last_error, sizeof(state->last_error), "Shell_NotifyIconW(NIM_ADD) failed");
    return 0;
  }
  state->visible = 1;
  moonbit_tray_set_message(state->last_error, sizeof(state->last_error), "");
  return 1;
#else
  (void)state;
  (void)tooltip;
  return 0;
#endif
}

MOONBIT_FFI_EXPORT int32_t moonbit_tray_hide(int64_t handle) {
  moonbit_tray_state_t *state = moonbit_tray_from_handle(handle);
#ifdef _WIN32
  if (state == NULL) {
    return 0;
  }
  if (!state->visible) {
    return 1;
  }
  if (!Shell_NotifyIconW(NIM_DELETE, &state->icon_data)) {
    moonbit_tray_set_message(state->last_error, sizeof(state->last_error), "Shell_NotifyIconW(NIM_DELETE) failed");
    return 0;
  }
  state->visible = 0;
  moonbit_tray_set_message(state->last_error, sizeof(state->last_error), "");
  return 1;
#else
  (void)state;
  return 0;
#endif
}

MOONBIT_FFI_EXPORT int32_t moonbit_tray_set_tooltip(
    int64_t handle,
    moonbit_bytes_t tooltip) {
  moonbit_tray_state_t *state = moonbit_tray_from_handle(handle);
#ifdef _WIN32
  if (state == NULL) {
    return 0;
  }
  moonbit_tray_copy_tooltip(state, tooltip);
  if (!state->visible) {
    moonbit_tray_set_message(state->last_error, sizeof(state->last_error), "");
    return 1;
  }
  if (!Shell_NotifyIconW(NIM_MODIFY, &state->icon_data)) {
    moonbit_tray_set_message(state->last_error, sizeof(state->last_error), "Shell_NotifyIconW(NIM_MODIFY) failed");
    return 0;
  }
  moonbit_tray_set_message(state->last_error, sizeof(state->last_error), "");
  return 1;
#else
  (void)state;
  (void)tooltip;
  return 0;
#endif
}

MOONBIT_FFI_EXPORT int32_t moonbit_tray_set_icon(
    int64_t handle,
    moonbit_bytes_t icon) {
  moonbit_tray_state_t *state = moonbit_tray_from_handle(handle);
#ifdef _WIN32
  if (state == NULL) {
    return 0;
  }
  if (!moonbit_tray_replace_icon(state, icon)) {
    return 0;
  }
  if (!state->visible) {
    moonbit_tray_set_message(state->last_error, sizeof(state->last_error), "");
    return 1;
  }
  if (!Shell_NotifyIconW(NIM_MODIFY, &state->icon_data)) {
    moonbit_tray_set_message(state->last_error, sizeof(state->last_error), "Shell_NotifyIconW(NIM_MODIFY) failed");
    return 0;
  }
  moonbit_tray_set_message(state->last_error, sizeof(state->last_error), "");
  return 1;
#else
  (void)state;
  (void)icon;
  return 0;
#endif
}

MOONBIT_FFI_EXPORT int32_t moonbit_tray_pump(
    int64_t handle,
    int32_t blocking) {
  moonbit_tray_state_t *state = moonbit_tray_from_handle(handle);
#ifdef _WIN32
  MSG message;
  BOOL has_message;
  (void)state;
  if (blocking) {
    has_message = GetMessageW(&message, NULL, 0, 0);
    if (has_message <= 0) {
      return 0;
    }
  } else {
    has_message = PeekMessageW(&message, NULL, 0, 0, PM_REMOVE);
    if (!has_message) {
      return 1;
    }
  }
  TranslateMessage(&message);
  DispatchMessageW(&message);
  return 1;
#else
  (void)state;
  (void)blocking;
  return -1;
#endif
}

MOONBIT_FFI_EXPORT moonbit_bytes_t moonbit_tray_last_error(
    int64_t handle) {
  moonbit_tray_state_t *state = moonbit_tray_from_handle(handle);
  if (state == NULL) {
    return moonbit_tray_copy_message("tray state is null");
  }
  return moonbit_tray_copy_message(state->last_error);
}
