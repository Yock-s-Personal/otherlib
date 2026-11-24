/**
 * \file ui/folder_dialog.hpp
 **/
#ifndef OTHERLIB_UI_FOLDER_DIALOG_HPP
#define OTHERLIB_UI_FOLDER_DIALOG_HPP

#include <cstdint>

struct SDL_Window;

namespace other {
  namespace ui {

    /// filelist ends in a null: its first entry is the folder, as UTF-8, or null when the
    ///  dialog was cancelled
    using folder_dialog_callback_fn = void (*)(void* userdata, const char* const* filelist, int32_t filter);

    /// the system's own folder picker, over that window. the callback is called once, from
    ///  a thread of the dialog's own, so what it writes has to be guarded
    void show_folder_dialog(SDL_Window* owner, folder_dialog_callback_fn callback, void* user_data);

  }  // namespace ui
}  // namespace other

#endif  // OTHERLIB_UI_FOLDER_DIALOG_HPP
