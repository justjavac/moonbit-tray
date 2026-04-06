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
#elif defined(__linux__)
#include <dlfcn.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#include <limits.h>
#include <pthread.h>
#endif

#include "moonbit.h"

typedef struct moonbit_tray_state {
#ifdef _WIN32
  HWND hwnd;
  NOTIFYICONDATAW icon_data;
#elif defined(__linux__)
  void *indicator;
  void *menu;
#elif defined(__APPLE__)
  void *pool;
  void *app;
  void *status_bar;
  void *status_item;
  void *button;
#endif
  int32_t visible;
  char last_error[256];
} moonbit_tray_state_t;

static char moonbit_tray_create_error[256];
static char moonbit_tray_support_message[256];

static moonbit_tray_state_t *moonbit_tray_from_handle(int64_t handle) {
  return (moonbit_tray_state_t *)(uintptr_t)handle;
}

static int64_t moonbit_tray_to_handle(moonbit_tray_state_t *state) {
  return (int64_t)(uintptr_t)state;
}

static const char *moonbit_tray_text_or(const char *value, const char *fallback) {
  if (value == NULL || value[0] == '\0') {
    return fallback;
  }
  return value;
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

static void moonbit_tray_clear_message(char *buffer, size_t size) {
  moonbit_tray_set_message(buffer, size, "");
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

#if defined(__linux__)
typedef struct moonbit_tray_linux_backend {
  int32_t initialized;
  void *gtk_lib;
  void *indicator_lib;
  int (*gtk_init_check)(int *, char ***);
  void *(*gtk_menu_new)(void);
  int (*gtk_main_iteration_do)(int);
  void (*g_object_unref)(void *);
  void *(*app_indicator_new)(const char *, const char *, int);
  void (*app_indicator_set_status)(void *, int);
  void (*app_indicator_set_menu)(void *, void *);
  void (*app_indicator_set_icon)(void *, const char *);
  void (*app_indicator_set_icon_full)(void *, const char *, const char *);
  void (*app_indicator_set_title)(void *, const char *);
} moonbit_tray_linux_backend_t;

enum {
  MOONBIT_TRAY_APPINDICATOR_CATEGORY_APPLICATION_STATUS = 0,
  MOONBIT_TRAY_APPINDICATOR_STATUS_PASSIVE = 0,
  MOONBIT_TRAY_APPINDICATOR_STATUS_ACTIVE = 1,
};

static moonbit_tray_linux_backend_t moonbit_tray_linux_backend;

static int32_t moonbit_tray_linux_open_library(
    const char *const *names,
    void **out_handle) {
  const char *const *name = names;
  for (; *name != NULL; ++name) {
    *out_handle = dlopen(*name, RTLD_LAZY | RTLD_GLOBAL);
    if (*out_handle != NULL) {
      return 1;
    }
  }
  return 0;
}

static int32_t moonbit_tray_linux_load_symbol(
    void **out_symbol,
    void *library,
    const char *name,
    int32_t required) {
  *out_symbol = dlsym(library, name);
  if (*out_symbol == NULL && required) {
    char message[256];
    snprintf(message, sizeof(message), "failed to resolve %s", name);
    moonbit_tray_set_message(
        moonbit_tray_support_message,
        sizeof(moonbit_tray_support_message),
        message);
    return 0;
  }
  return 1;
}

static int32_t moonbit_tray_linux_backend_init(void) {
  static const char *const gtk_names[] = {
      "libgtk-3.so.0",
      "libgtk-3.so",
      NULL,
  };
  static const char *const indicator_names[] = {
      "libayatana-appindicator3.so.1",
      "libayatana-appindicator3.so",
      "libappindicator3.so.1",
      "libappindicator3.so",
      NULL,
  };
  if (moonbit_tray_linux_backend.initialized > 0) {
    return 1;
  }
  if (moonbit_tray_linux_backend.initialized < 0) {
    return 0;
  }
  if (!moonbit_tray_linux_open_library(
          gtk_names,
          &moonbit_tray_linux_backend.gtk_lib)) {
    moonbit_tray_set_message(
        moonbit_tray_support_message,
        sizeof(moonbit_tray_support_message),
        "GTK 3 runtime not found (expected libgtk-3)");
    moonbit_tray_linux_backend.initialized = -1;
    return 0;
  }
  if (!moonbit_tray_linux_open_library(
          indicator_names,
          &moonbit_tray_linux_backend.indicator_lib)) {
    moonbit_tray_set_message(
        moonbit_tray_support_message,
        sizeof(moonbit_tray_support_message),
        "AppIndicator runtime not found (expected libayatana-appindicator3 or libappindicator3)");
    moonbit_tray_linux_backend.initialized = -1;
    return 0;
  }
  if (!moonbit_tray_linux_load_symbol(
          (void **)&moonbit_tray_linux_backend.gtk_init_check,
          moonbit_tray_linux_backend.gtk_lib,
          "gtk_init_check",
          1) ||
      !moonbit_tray_linux_load_symbol(
          (void **)&moonbit_tray_linux_backend.gtk_menu_new,
          moonbit_tray_linux_backend.gtk_lib,
          "gtk_menu_new",
          1) ||
      !moonbit_tray_linux_load_symbol(
          (void **)&moonbit_tray_linux_backend.gtk_main_iteration_do,
          moonbit_tray_linux_backend.gtk_lib,
          "gtk_main_iteration_do",
          1) ||
      !moonbit_tray_linux_load_symbol(
          (void **)&moonbit_tray_linux_backend.app_indicator_new,
          moonbit_tray_linux_backend.indicator_lib,
          "app_indicator_new",
          1) ||
      !moonbit_tray_linux_load_symbol(
          (void **)&moonbit_tray_linux_backend.app_indicator_set_status,
          moonbit_tray_linux_backend.indicator_lib,
          "app_indicator_set_status",
          1) ||
      !moonbit_tray_linux_load_symbol(
          (void **)&moonbit_tray_linux_backend.app_indicator_set_menu,
          moonbit_tray_linux_backend.indicator_lib,
          "app_indicator_set_menu",
          1)) {
    moonbit_tray_linux_backend.initialized = -1;
    return 0;
  }
  moonbit_tray_linux_load_symbol(
      (void **)&moonbit_tray_linux_backend.app_indicator_set_icon_full,
      moonbit_tray_linux_backend.indicator_lib,
      "app_indicator_set_icon_full",
      0);
  moonbit_tray_linux_load_symbol(
      (void **)&moonbit_tray_linux_backend.app_indicator_set_icon,
      moonbit_tray_linux_backend.indicator_lib,
      "app_indicator_set_icon",
      0);
  moonbit_tray_linux_load_symbol(
      (void **)&moonbit_tray_linux_backend.app_indicator_set_title,
      moonbit_tray_linux_backend.indicator_lib,
      "app_indicator_set_title",
      0);
  moonbit_tray_linux_backend.g_object_unref =
      (void (*)(void *))dlsym(RTLD_DEFAULT, "g_object_unref");
  if (!moonbit_tray_linux_backend.gtk_init_check(NULL, NULL)) {
    moonbit_tray_set_message(
        moonbit_tray_support_message,
        sizeof(moonbit_tray_support_message),
        "GTK initialization failed; make sure a desktop session is available");
    moonbit_tray_linux_backend.initialized = -1;
    return 0;
  }
  moonbit_tray_clear_message(
      moonbit_tray_support_message,
      sizeof(moonbit_tray_support_message));
  moonbit_tray_linux_backend.initialized = 1;
  return 1;
}

static int32_t moonbit_tray_linux_apply_icon(
    moonbit_tray_state_t *state,
    moonbit_bytes_t icon) {
  const char *icon_name =
      moonbit_tray_text_or((const char *)icon, "applications-system");
  if (moonbit_tray_linux_backend.app_indicator_set_icon_full != NULL) {
    moonbit_tray_linux_backend.app_indicator_set_icon_full(
        state->indicator,
        icon_name,
        "");
  } else if (moonbit_tray_linux_backend.app_indicator_set_icon != NULL) {
    moonbit_tray_linux_backend.app_indicator_set_icon(
        state->indicator,
        icon_name);
  } else {
    moonbit_tray_set_message(
        state->last_error,
        sizeof(state->last_error),
        "no AppIndicator icon setter is available");
    return 0;
  }
  moonbit_tray_clear_message(state->last_error, sizeof(state->last_error));
  return 1;
}

static void moonbit_tray_linux_apply_tooltip(
    moonbit_tray_state_t *state,
    moonbit_bytes_t tooltip) {
  if (moonbit_tray_linux_backend.app_indicator_set_title != NULL) {
    moonbit_tray_linux_backend.app_indicator_set_title(
        state->indicator,
        moonbit_tray_text_or((const char *)tooltip, ""));
  }
}
#endif

#if defined(__APPLE__)
typedef void *moonbit_tray_id;
typedef void *moonbit_tray_sel;
typedef signed char moonbit_tray_bool;

typedef struct moonbit_tray_macos_backend {
  int32_t initialized;
  void *objc_lib;
  void *appkit_lib;
  moonbit_tray_id (*objc_getClass)(const char *);
  moonbit_tray_sel (*sel_registerName)(const char *);
  void *objc_msgSend;
} moonbit_tray_macos_backend_t;

static moonbit_tray_macos_backend_t moonbit_tray_macos_backend;

static int32_t moonbit_tray_macos_backend_init(void) {
  if (moonbit_tray_macos_backend.initialized > 0) {
    return 1;
  }
  if (moonbit_tray_macos_backend.initialized < 0) {
    return 0;
  }
  moonbit_tray_macos_backend.objc_lib =
      dlopen("/usr/lib/libobjc.A.dylib", RTLD_LAZY | RTLD_GLOBAL);
  if (moonbit_tray_macos_backend.objc_lib == NULL) {
    moonbit_tray_macos_backend.objc_lib =
        dlopen("/usr/lib/libobjc.dylib", RTLD_LAZY | RTLD_GLOBAL);
  }
  moonbit_tray_macos_backend.appkit_lib =
      dlopen(
          "/System/Library/Frameworks/AppKit.framework/AppKit",
          RTLD_LAZY | RTLD_GLOBAL);
  if (moonbit_tray_macos_backend.objc_lib == NULL ||
      moonbit_tray_macos_backend.appkit_lib == NULL) {
    moonbit_tray_set_message(
        moonbit_tray_support_message,
        sizeof(moonbit_tray_support_message),
        "AppKit or Objective-C runtime could not be loaded");
    moonbit_tray_macos_backend.initialized = -1;
    return 0;
  }
  moonbit_tray_macos_backend.objc_getClass =
      (moonbit_tray_id(*)(const char *))dlsym(
          moonbit_tray_macos_backend.objc_lib,
          "objc_getClass");
  moonbit_tray_macos_backend.sel_registerName =
      (moonbit_tray_sel(*)(const char *))dlsym(
          moonbit_tray_macos_backend.objc_lib,
          "sel_registerName");
  moonbit_tray_macos_backend.objc_msgSend =
      dlsym(moonbit_tray_macos_backend.objc_lib, "objc_msgSend");
  if (moonbit_tray_macos_backend.objc_getClass == NULL ||
      moonbit_tray_macos_backend.sel_registerName == NULL ||
      moonbit_tray_macos_backend.objc_msgSend == NULL) {
    moonbit_tray_set_message(
        moonbit_tray_support_message,
        sizeof(moonbit_tray_support_message),
        "failed to resolve Objective-C runtime entry points");
    moonbit_tray_macos_backend.initialized = -1;
    return 0;
  }
  moonbit_tray_clear_message(
      moonbit_tray_support_message,
      sizeof(moonbit_tray_support_message));
  moonbit_tray_macos_backend.initialized = 1;
  return 1;
}

static moonbit_tray_sel moonbit_tray_macos_sel(const char *name) {
  return moonbit_tray_macos_backend.sel_registerName(name);
}

static moonbit_tray_id moonbit_tray_macos_class(const char *name) {
  return moonbit_tray_macos_backend.objc_getClass(name);
}

static moonbit_tray_id moonbit_tray_macos_send_id(
    moonbit_tray_id object,
    const char *selector_name) {
  return ((moonbit_tray_id(*)(moonbit_tray_id, moonbit_tray_sel))
              moonbit_tray_macos_backend.objc_msgSend)(
      object,
      moonbit_tray_macos_sel(selector_name));
}

static moonbit_tray_id moonbit_tray_macos_send_id_id(
    moonbit_tray_id object,
    const char *selector_name,
    moonbit_tray_id arg) {
  return ((moonbit_tray_id(*)(moonbit_tray_id, moonbit_tray_sel, moonbit_tray_id))
              moonbit_tray_macos_backend.objc_msgSend)(
      object,
      moonbit_tray_macos_sel(selector_name),
      arg);
}

static moonbit_tray_id moonbit_tray_macos_send_id_cstring(
    moonbit_tray_id object,
    const char *selector_name,
    const char *arg) {
  return ((moonbit_tray_id(*)(moonbit_tray_id, moonbit_tray_sel, const char *))
              moonbit_tray_macos_backend.objc_msgSend)(
      object,
      moonbit_tray_macos_sel(selector_name),
      arg);
}

static moonbit_tray_id moonbit_tray_macos_send_id_double(
    moonbit_tray_id object,
    const char *selector_name,
    double arg) {
  return ((moonbit_tray_id(*)(moonbit_tray_id, moonbit_tray_sel, double))
              moonbit_tray_macos_backend.objc_msgSend)(
      object,
      moonbit_tray_macos_sel(selector_name),
      arg);
}

static moonbit_tray_id moonbit_tray_macos_send_id_ulong_id_id_bool(
    moonbit_tray_id object,
    const char *selector_name,
    unsigned long arg1,
    moonbit_tray_id arg2,
    moonbit_tray_id arg3,
    moonbit_tray_bool arg4) {
  return ((moonbit_tray_id(*)(moonbit_tray_id, moonbit_tray_sel, unsigned long, moonbit_tray_id, moonbit_tray_id, moonbit_tray_bool))
              moonbit_tray_macos_backend.objc_msgSend)(
      object,
      moonbit_tray_macos_sel(selector_name),
      arg1,
      arg2,
      arg3,
      arg4);
}

static void moonbit_tray_macos_send_void(
    moonbit_tray_id object,
    const char *selector_name) {
  ((void (*)(moonbit_tray_id, moonbit_tray_sel))
       moonbit_tray_macos_backend.objc_msgSend)(
      object,
      moonbit_tray_macos_sel(selector_name));
}

static void moonbit_tray_macos_send_void_id(
    moonbit_tray_id object,
    const char *selector_name,
    moonbit_tray_id arg) {
  ((void (*)(moonbit_tray_id, moonbit_tray_sel, moonbit_tray_id))
       moonbit_tray_macos_backend.objc_msgSend)(
      object,
      moonbit_tray_macos_sel(selector_name),
      arg);
}

static void moonbit_tray_macos_send_void_bool(
    moonbit_tray_id object,
    const char *selector_name,
    moonbit_tray_bool arg) {
  ((void (*)(moonbit_tray_id, moonbit_tray_sel, moonbit_tray_bool))
       moonbit_tray_macos_backend.objc_msgSend)(
      object,
      moonbit_tray_macos_sel(selector_name),
      arg);
}

static moonbit_tray_id moonbit_tray_macos_string(const char *value) {
  return moonbit_tray_macos_send_id_cstring(
      moonbit_tray_macos_class("NSString"),
      "stringWithUTF8String:",
      moonbit_tray_text_or(value, ""));
}

static int32_t moonbit_tray_macos_apply_icon(
    moonbit_tray_state_t *state,
    moonbit_bytes_t icon) {
  const char *icon_text = (const char *)icon;
  moonbit_tray_id button =
      state->button != NULL ? state->button : state->status_item;
  if (button == NULL) {
    moonbit_tray_set_message(
        state->last_error,
        sizeof(state->last_error),
        "status item button is unavailable");
    return 0;
  }
  if (icon_text != NULL && icon_text[0] != '\0') {
    moonbit_tray_id path = moonbit_tray_macos_string(icon_text);
    moonbit_tray_id image = moonbit_tray_macos_send_id_id(
        moonbit_tray_macos_send_id(
            moonbit_tray_macos_class("NSImage"),
            "alloc"),
        "initWithContentsOfFile:",
        path);
    if (image == NULL) {
      image = moonbit_tray_macos_send_id_id(
          moonbit_tray_macos_class("NSImage"),
          "imageNamed:",
          path);
    }
    if (image != NULL) {
      moonbit_tray_macos_send_void_id(button, "setImage:", image);
      moonbit_tray_macos_send_void_id(
          button,
          "setTitle:",
          moonbit_tray_macos_string(""));
      moonbit_tray_clear_message(state->last_error, sizeof(state->last_error));
      return 1;
    }
  }
  moonbit_tray_macos_send_void_id(button, "setImage:", NULL);
  moonbit_tray_macos_send_void_id(
      button,
      "setTitle:",
      moonbit_tray_macos_string("Tray"));
  moonbit_tray_clear_message(state->last_error, sizeof(state->last_error));
  return 1;
}

static void moonbit_tray_macos_apply_tooltip(
    moonbit_tray_state_t *state,
    moonbit_bytes_t tooltip) {
  moonbit_tray_id button =
      state->button != NULL ? state->button : state->status_item;
  if (button != NULL) {
    moonbit_tray_macos_send_void_id(
        button,
        "setToolTip:",
        moonbit_tray_macos_string((const char *)tooltip));
  }
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
  return moonbit_tray_ensure_window_class();
#elif defined(__linux__)
  return moonbit_tray_linux_backend_init();
#elif defined(__APPLE__)
  return moonbit_tray_macos_backend_init();
#else
  moonbit_tray_set_message(
      moonbit_tray_support_message,
      sizeof(moonbit_tray_support_message),
      "tray support is not available on this operating system");
  return 0;
#endif
}

MOONBIT_FFI_EXPORT moonbit_bytes_t moonbit_tray_support_error(void) {
  return moonbit_tray_copy_message(moonbit_tray_support_message);
}

MOONBIT_FFI_EXPORT int64_t moonbit_tray_create(
    moonbit_bytes_t identifier,
    moonbit_bytes_t icon,
    moonbit_bytes_t tooltip) {
  moonbit_tray_state_t *state;
#ifdef _WIN32
  if (!moonbit_tray_ensure_window_class()) {
    moonbit_tray_set_message(
        moonbit_tray_create_error,
        sizeof(moonbit_tray_create_error),
        moonbit_tray_support_message);
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
  moonbit_tray_clear_message(state->last_error, sizeof(state->last_error));
  moonbit_tray_clear_message(
      moonbit_tray_create_error,
      sizeof(moonbit_tray_create_error));
  return moonbit_tray_to_handle(state);
#elif defined(__linux__)
  if (!moonbit_tray_linux_backend_init()) {
    moonbit_tray_set_message(
        moonbit_tray_create_error,
        sizeof(moonbit_tray_create_error),
        moonbit_tray_support_message);
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
  state->menu = moonbit_tray_linux_backend.gtk_menu_new();
  if (state->menu == NULL) {
    moonbit_tray_set_message(
        moonbit_tray_create_error,
        sizeof(moonbit_tray_create_error),
        "failed to create the GTK tray menu");
    free(state);
    return 0;
  }
  state->indicator = moonbit_tray_linux_backend.app_indicator_new(
      moonbit_tray_text_or((const char *)identifier, "moonbit-tray"),
      moonbit_tray_text_or((const char *)icon, "applications-system"),
      MOONBIT_TRAY_APPINDICATOR_CATEGORY_APPLICATION_STATUS);
  if (state->indicator == NULL) {
    moonbit_tray_set_message(
        moonbit_tray_create_error,
        sizeof(moonbit_tray_create_error),
        "failed to create the AppIndicator instance");
    if (moonbit_tray_linux_backend.g_object_unref != NULL) {
      moonbit_tray_linux_backend.g_object_unref(state->menu);
    }
    free(state);
    return 0;
  }
  moonbit_tray_linux_backend.app_indicator_set_menu(state->indicator, state->menu);
  moonbit_tray_linux_backend.app_indicator_set_status(
      state->indicator,
      MOONBIT_TRAY_APPINDICATOR_STATUS_PASSIVE);
  if (!moonbit_tray_linux_apply_icon(state, icon)) {
    moonbit_tray_set_message(
        moonbit_tray_create_error,
        sizeof(moonbit_tray_create_error),
        state->last_error);
    if (moonbit_tray_linux_backend.g_object_unref != NULL) {
      moonbit_tray_linux_backend.g_object_unref(state->indicator);
      moonbit_tray_linux_backend.g_object_unref(state->menu);
    }
    free(state);
    return 0;
  }
  moonbit_tray_linux_apply_tooltip(state, tooltip);
  moonbit_tray_clear_message(state->last_error, sizeof(state->last_error));
  moonbit_tray_clear_message(
      moonbit_tray_create_error,
      sizeof(moonbit_tray_create_error));
  return moonbit_tray_to_handle(state);
#elif defined(__APPLE__)
  if (!moonbit_tray_macos_backend_init()) {
    moonbit_tray_set_message(
        moonbit_tray_create_error,
        sizeof(moonbit_tray_create_error),
        moonbit_tray_support_message);
    return 0;
  }
  if (pthread_main_np() == 0) {
    moonbit_tray_set_message(
        moonbit_tray_create_error,
        sizeof(moonbit_tray_create_error),
        "macOS tray must be created on the main thread");
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
  state->pool = moonbit_tray_macos_send_id(
      moonbit_tray_macos_class("NSAutoreleasePool"),
      "new");
  state->app = moonbit_tray_macos_send_id(
      moonbit_tray_macos_class("NSApplication"),
      "sharedApplication");
  state->status_bar = moonbit_tray_macos_send_id(
      moonbit_tray_macos_class("NSStatusBar"),
      "systemStatusBar");
  state->status_item = moonbit_tray_macos_send_id_double(
      state->status_bar,
      "statusItemWithLength:",
      -1.0);
  if (state->status_item == NULL) {
    moonbit_tray_set_message(
        moonbit_tray_create_error,
        sizeof(moonbit_tray_create_error),
        "failed to create the NSStatusItem");
    free(state);
    return 0;
  }
  moonbit_tray_macos_send_id(state->status_item, "retain");
  moonbit_tray_macos_send_void_bool(
      state->status_item,
      "setHighlightMode:",
      (moonbit_tray_bool)1);
  state->button = moonbit_tray_macos_send_id(state->status_item, "button");
  moonbit_tray_macos_send_void_bool(
      state->app,
      "activateIgnoringOtherApps:",
      (moonbit_tray_bool)1);
  if (!moonbit_tray_macos_apply_icon(state, icon)) {
    moonbit_tray_set_message(
        moonbit_tray_create_error,
        sizeof(moonbit_tray_create_error),
        state->last_error);
    moonbit_tray_macos_send_void_id(
        state->status_bar,
        "removeStatusItem:",
        state->status_item);
    free(state);
    return 0;
  }
  moonbit_tray_macos_apply_tooltip(state, tooltip);
  moonbit_tray_macos_send_void_bool(
      state->status_item,
      "setVisible:",
      (moonbit_tray_bool)0);
  moonbit_tray_clear_message(state->last_error, sizeof(state->last_error));
  moonbit_tray_clear_message(
      moonbit_tray_create_error,
      sizeof(moonbit_tray_create_error));
  return moonbit_tray_to_handle(state);
#else
  moonbit_tray_set_message(
      moonbit_tray_create_error,
      sizeof(moonbit_tray_create_error),
      "native tray backend is unavailable on this platform");
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
#elif defined(__linux__)
  if (state->indicator != NULL) {
    moonbit_tray_linux_backend.app_indicator_set_status(
        state->indicator,
        MOONBIT_TRAY_APPINDICATOR_STATUS_PASSIVE);
  }
  if (moonbit_tray_linux_backend.g_object_unref != NULL) {
    if (state->indicator != NULL) {
      moonbit_tray_linux_backend.g_object_unref(state->indicator);
    }
    if (state->menu != NULL) {
      moonbit_tray_linux_backend.g_object_unref(state->menu);
    }
  }
#elif defined(__APPLE__)
  if (state->status_item != NULL && state->status_bar != NULL) {
    moonbit_tray_macos_send_void_id(
        state->status_bar,
        "removeStatusItem:",
        state->status_item);
    moonbit_tray_macos_send_void(state->status_item, "release");
  }
  if (state->pool != NULL) {
    moonbit_tray_macos_send_void(state->pool, "drain");
  }
#endif
  free(state);
}

MOONBIT_FFI_EXPORT int32_t moonbit_tray_show(
    int64_t handle,
    moonbit_bytes_t tooltip) {
  moonbit_tray_state_t *state = moonbit_tray_from_handle(handle);
  if (state == NULL) {
    return 0;
  }
#ifdef _WIN32
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
  moonbit_tray_clear_message(state->last_error, sizeof(state->last_error));
  return 1;
#elif defined(__linux__)
  moonbit_tray_linux_apply_tooltip(state, tooltip);
  moonbit_tray_linux_backend.app_indicator_set_status(
      state->indicator,
      MOONBIT_TRAY_APPINDICATOR_STATUS_ACTIVE);
  state->visible = 1;
  moonbit_tray_clear_message(state->last_error, sizeof(state->last_error));
  return 1;
#elif defined(__APPLE__)
  moonbit_tray_macos_apply_tooltip(state, tooltip);
  moonbit_tray_macos_send_void_bool(
      state->status_item,
      "setVisible:",
      (moonbit_tray_bool)1);
  state->visible = 1;
  moonbit_tray_clear_message(state->last_error, sizeof(state->last_error));
  return 1;
#else
  (void)tooltip;
  return 0;
#endif
}

MOONBIT_FFI_EXPORT int32_t moonbit_tray_hide(int64_t handle) {
  moonbit_tray_state_t *state = moonbit_tray_from_handle(handle);
  if (state == NULL) {
    return 0;
  }
#ifdef _WIN32
  if (!state->visible) {
    return 1;
  }
  if (!Shell_NotifyIconW(NIM_DELETE, &state->icon_data)) {
    moonbit_tray_set_message(state->last_error, sizeof(state->last_error), "Shell_NotifyIconW(NIM_DELETE) failed");
    return 0;
  }
  state->visible = 0;
  moonbit_tray_clear_message(state->last_error, sizeof(state->last_error));
  return 1;
#elif defined(__linux__)
  moonbit_tray_linux_backend.app_indicator_set_status(
      state->indicator,
      MOONBIT_TRAY_APPINDICATOR_STATUS_PASSIVE);
  state->visible = 0;
  moonbit_tray_clear_message(state->last_error, sizeof(state->last_error));
  return 1;
#elif defined(__APPLE__)
  moonbit_tray_macos_send_void_bool(
      state->status_item,
      "setVisible:",
      (moonbit_tray_bool)0);
  state->visible = 0;
  moonbit_tray_clear_message(state->last_error, sizeof(state->last_error));
  return 1;
#else
  return 0;
#endif
}

MOONBIT_FFI_EXPORT int32_t moonbit_tray_set_tooltip(
    int64_t handle,
    moonbit_bytes_t tooltip) {
  moonbit_tray_state_t *state = moonbit_tray_from_handle(handle);
  if (state == NULL) {
    return 0;
  }
#ifdef _WIN32
  moonbit_tray_copy_tooltip(state, tooltip);
  if (!state->visible) {
    moonbit_tray_clear_message(state->last_error, sizeof(state->last_error));
    return 1;
  }
  if (!Shell_NotifyIconW(NIM_MODIFY, &state->icon_data)) {
    moonbit_tray_set_message(state->last_error, sizeof(state->last_error), "Shell_NotifyIconW(NIM_MODIFY) failed");
    return 0;
  }
  moonbit_tray_clear_message(state->last_error, sizeof(state->last_error));
  return 1;
#elif defined(__linux__)
  moonbit_tray_linux_apply_tooltip(state, tooltip);
  moonbit_tray_clear_message(state->last_error, sizeof(state->last_error));
  return 1;
#elif defined(__APPLE__)
  moonbit_tray_macos_apply_tooltip(state, tooltip);
  moonbit_tray_clear_message(state->last_error, sizeof(state->last_error));
  return 1;
#else
  (void)tooltip;
  return 0;
#endif
}

MOONBIT_FFI_EXPORT int32_t moonbit_tray_set_icon(
    int64_t handle,
    moonbit_bytes_t icon) {
  moonbit_tray_state_t *state = moonbit_tray_from_handle(handle);
  if (state == NULL) {
    return 0;
  }
#ifdef _WIN32
  if (!moonbit_tray_replace_icon(state, icon)) {
    return 0;
  }
  if (!state->visible) {
    moonbit_tray_clear_message(state->last_error, sizeof(state->last_error));
    return 1;
  }
  if (!Shell_NotifyIconW(NIM_MODIFY, &state->icon_data)) {
    moonbit_tray_set_message(state->last_error, sizeof(state->last_error), "Shell_NotifyIconW(NIM_MODIFY) failed");
    return 0;
  }
  moonbit_tray_clear_message(state->last_error, sizeof(state->last_error));
  return 1;
#elif defined(__linux__)
  return moonbit_tray_linux_apply_icon(state, icon);
#elif defined(__APPLE__)
  return moonbit_tray_macos_apply_icon(state, icon);
#else
  (void)icon;
  return 0;
#endif
}

MOONBIT_FFI_EXPORT int32_t moonbit_tray_pump(
    int64_t handle,
    int32_t blocking) {
  moonbit_tray_state_t *state = moonbit_tray_from_handle(handle);
  if (state == NULL) {
    return -1;
  }
#ifdef _WIN32
  MSG message;
  BOOL has_message;
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
#elif defined(__linux__)
  (void)state;
  moonbit_tray_linux_backend.gtk_main_iteration_do(blocking ? 1 : 0);
  return 1;
#elif defined(__APPLE__)
  {
    moonbit_tray_id until = blocking
        ? moonbit_tray_macos_send_id(
              moonbit_tray_macos_class("NSDate"),
              "distantFuture")
        : moonbit_tray_macos_send_id(
              moonbit_tray_macos_class("NSDate"),
              "distantPast");
    moonbit_tray_id event = moonbit_tray_macos_send_id_ulong_id_id_bool(
        state->app,
        "nextEventMatchingMask:untilDate:inMode:dequeue:",
        ULONG_MAX,
        until,
        moonbit_tray_macos_string("kCFRunLoopDefaultMode"),
        (moonbit_tray_bool)1);
    if (event != NULL) {
      moonbit_tray_macos_send_void_id(state->app, "sendEvent:", event);
    }
    return 1;
  }
#else
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
