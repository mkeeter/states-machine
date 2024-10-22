#define _WIN32_WINNT _WIN32_WINNT_WIN7
#include <windows.h>
#include <shlwapi.h>

// Used for SHGetKnownFolderPath
#include <Shlobj.h>

// Mystery includes, necessary to get FOLDERID_RoamingAppData linked
#include <initguid.h>
#include <knownfolders.h>

#include "instance.h"
#include "log.h"
#include "object.h"
#include "platform.h"
#include "window.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

int64_t platform_get_time(void) {
    FILETIME t;
    GetSystemTimePreciseAsFileTime(&t);
    LARGE_INTEGER i;
    i.LowPart = t.dwLowDateTime;
    i.HighPart = t.dwHighDateTime;
    return i.QuadPart / 10;
}

void platform_set_terminal_color(FILE* f, platform_terminal_color_t c) {
    (void)f;
    (void)c;
}

void platform_clear_terminal_color(FILE* f) {
    (void)f;
}

struct platform_mutex_ {
    CRITICAL_SECTION data;
};

platform_mutex_t* platform_mutex_new() {
    OBJECT_ALLOC(platform_mutex);
    InitializeCriticalSection(&platform_mutex->data);
    return platform_mutex;
}

void platform_mutex_delete(platform_mutex_t* mutex) {
    DeleteCriticalSection(&mutex->data);
    free(mutex);
}

int platform_mutex_lock(platform_mutex_t* mutex) {
    EnterCriticalSection(&mutex->data);
    return 0;
}

int platform_mutex_unlock(platform_mutex_t* mutex) {
    LeaveCriticalSection(&mutex->data);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

struct platform_cond_ {
    CONDITION_VARIABLE data;
};

platform_cond_t* platform_cond_new() {
    OBJECT_ALLOC(platform_cond);
    InitializeConditionVariable(&platform_cond->data);
    return platform_cond;
}

void platform_cond_delete(platform_cond_t* cond) {
    free(cond);
}

int platform_cond_wait(platform_cond_t* cond,
                       platform_mutex_t* mutex)
{
    return !SleepConditionVariableCS(&cond->data, &mutex->data, INFINITE);
}

int platform_cond_broadcast(platform_cond_t* cond) {
    WakeAllConditionVariable(&cond->data);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

struct platform_thread_ {
    HANDLE data;
};

/* The pthreads API (and thus the platform_thread API) expects a function
 * returning void*, while the win32 API returns DWORD.  We use this small
 * struct to translate from one to the other. */
typedef struct {
    void *(*run)(void *);
    void* data;
} thread_data_t;

DWORD WINAPI thread_runner(LPVOID data_) {
    thread_data_t* data = data_;
    (data->run)(data->data);
    free(data_);
    return 0;
}

platform_thread_t*  platform_thread_new(void *(*run)(void *),
                                        void* data)
{
    OBJECT_ALLOC(platform_thread);

    thread_data_t* d = calloc(1, sizeof(thread_data_t));
    d->run = run;
    d->data = data;

    platform_thread->data = CreateThread(NULL, 0, &thread_runner, d, 0, NULL);
    if (platform_thread->data == NULL) {
        free(platform_thread);
        return NULL;
    }
    return platform_thread;
}

void platform_thread_delete(platform_thread_t* thread) {
    CloseHandle(thread->data);
    free(thread);
}

int platform_thread_join(platform_thread_t* thread) {
    return WaitForSingleObject(thread->data, INFINITE);
}

////////////////////////////////////////////////////////////////////////////////

void platform_init(int argc, char** argv) {
    // Nothing to do on Windows
}

#define ID_FILE_EXIT            9001

/*  We hot-swap the WNDPROC pointer from the one defined in GLFW to our
 *  own here, which lets us respond to menu events (ignored in GLFW). */
static WNDPROC glfw_wndproc = NULL;
static LRESULT CALLBACK wndproc(HWND hWnd, UINT message,
                                WPARAM wParam, LPARAM lParam)
{
    if (message == WM_COMMAND) {
        switch(LOWORD(wParam)) {
            case ID_FILE_EXIT:
                SendMessage(hWnd, WM_CLOSE, 0, 0);
                break;
        }
    }
    return (*glfw_wndproc)(hWnd, message, wParam, lParam);
}

void platform_window_bind(GLFWwindow* window) {
    HWND w = glfwGetWin32Window(window);
    if (glfw_wndproc == NULL) {
        glfw_wndproc = (WNDPROC)GetWindowLongPtrW(w, GWLP_WNDPROC);
    }
    SetWindowLongPtrW(w, GWLP_WNDPROC, (LONG_PTR)wndproc);

    HMENU menu = CreateMenu();
    HMENU file = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, ID_FILE_EXIT, L"E&xit");
    AppendMenu(menu, MF_STRING | MF_POPUP, (UINT_PTR)file, "&File");

    SetMenu(w, menu);
}

/*  Returns the filename portion of a full path */
const char* platform_filename(const char* filepath) {
    return PathFindFileNameA(filepath);
}

const char* platform_get_user_file(const char* file) {
    PWSTR out = NULL;
    SHGetKnownFolderPath(
        &FOLDERID_RoamingAppData,
        KF_FLAG_CREATE,
        NULL,
        &out);

    PWSTR ptr = out;
    while (*ptr) {
        ++ptr;
    }
    const char* folder = "StatesMachine";
    char* s = malloc((ptr - out) + 1 + strlen(folder) + 1 + strlen(file) + 1);
    char* v = s;
    ptr = out;
    while (*ptr) {
        *v++ = *ptr++;
    }
    *v++ = '\\';
    while (*folder) {
        *v++ = *folder++;
    }
    *v = 0;
    CreateDirectoryA(s, NULL);

    // Append filename
    *v++ = '\\';
    while (*file) {
        *v++ = *file++;
    }
    *v = 0;
    return s;
}
