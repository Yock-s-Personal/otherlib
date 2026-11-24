/**
 * \file ui/folder_dialog.cpp
 **/
#include "ui/folder_dialog.hpp"

#include <string>
#include <thread>

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>

#include "core/defines.hpp"
#include "core/logger.hpp"

#ifdef OTHER_ENVIRONMENT_WINDOWS
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <shobjidl.h>
  #include <windows.h>
#endif

namespace other {
  namespace ui {

#ifdef OTHER_ENVIRONMENT_WINDOWS
    namespace {

      /// SDL's folder dialog on Windows is the old tree of folders. this is the one every
      ///  other program shows: Explorer's own window, with its address bar and its places.
      ///  it runs its own message loop, so it has a thread to itself
      void pick_folder(HWND owner, folder_dialog_callback_fn callback, void* user_data) {
        std::string picked = "";
        const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

        IFileOpenDialog* dialog = nullptr;
        if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
          CORE_LOG_ERROR("The system's folder dialog could not be made.");
        } else {
          FILEOPENDIALOGOPTIONS options = 0;
          dialog->GetOptions(&options);
          dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);

          /// Show fails when the dialog is cancelled, which is not an error
          IShellItem* item = nullptr;
          if (SUCCEEDED(dialog->Show(owner)) && SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR wide = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &wide))) {
              const int32_t size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
              if (size > 1) {
                picked.resize(static_cast<size_t>(size) - 1);
                WideCharToMultiByte(CP_UTF8, 0, wide, -1, picked.data(), size, nullptr, nullptr);
              }
              CoTaskMemFree(wide);
            }
            item->Release();
          }
          dialog->Release();
        }
        if (SUCCEEDED(com)) {
          CoUninitialize();
        }

        const char* const filelist[] = { picked.empty() ? nullptr : picked.c_str(), nullptr };
        callback(user_data, filelist, 0);
      }

    }  // namespace
#endif

    void show_folder_dialog(SDL_Window* owner, folder_dialog_callback_fn callback, void* user_data) {
      OTHER_ASSERT(callback != nullptr, "A folder dialog needs somewhere to answer.");
#ifdef OTHER_ENVIRONMENT_WINDOWS
      HWND window = nullptr;
      if (owner != nullptr) {
        window = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(owner), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
      }
      std::thread(pick_folder, window, callback, user_data).detach();
#else
      SDL_ShowOpenFolderDialog(callback, user_data, owner, nullptr, false);
#endif
    }

  }  // namespace ui
}  // namespace other
