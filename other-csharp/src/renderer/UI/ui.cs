using OtherCsBindings;
using Other.Core;
using Other.UI;
using System;
using System.Buffers;

namespace Other
{
  public static class UIBindings
  {
    [NativeFunction("BeginWindow")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, int, NativeBool32> NativeBeginWindow;

    [NativeFunction("EndWindow")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeEndWindow;

    [NativeFunction("BeginChild")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float, float, int, int, NativeBool32> NativeBeginChild;
    [NativeFunction("EndChild")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeEndChild;
    [NativeFunction("UIText")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, void> NativeText;
    [NativeFunction("UITextColored")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, float, float, float, byte*, void> NativeTextColored;
    [NativeFunction("UITextWrapped")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, void> NativeTextWrapped;
    [NativeFunction("UILabelText")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, byte*, void> NativeLabelText;
    [NativeFunction("UIButton")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float, float, NativeBool32> NativeButton;
    [NativeFunction("UISmallButton")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, NativeBool32> NativeSmallButton;
    [NativeFunction("UIInvisibleButton")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float, float, NativeBool32> NativeInvisibleButton;
    [NativeFunction("UICheckbox")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, NativeBool32*, NativeBool32> NativeCheckbox;
    [NativeFunction("UIRadioButton")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, NativeBool32, NativeBool32> NativeRadioButton;
    [NativeFunction("UIProgressBar")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, float, float, byte*, void> NativeProgressBar;
    [NativeFunction("UISeparator")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeSeparator;
    [NativeFunction("UISameLine")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, float, void> NativeSameLine;
    [NativeFunction("UISpacing")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeSpacing;
    [NativeFunction("UIIndent")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, void> NativeIndent;
    [NativeFunction("UIUnindent")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, void> NativeUnindent;
    [NativeFunction("UINewLine")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeNewLine;
    [NativeFunction("UIDummy")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, float, void> NativeDummy;
    [NativeFunction("UIInputText")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, byte*, int, int, NativeBool32> NativeInputText;
    [NativeFunction("UIInputTextMultiline")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, byte*, int, float, float, int, NativeBool32> NativeInputTextMultiline;
    [NativeFunction("UIInputFloat")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float*, float, float, int, NativeBool32> NativeInputFloat;
    [NativeFunction("UIInputFloat2")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float*, NativeBool32> NativeInputFloat2;
    [NativeFunction("UIInputFloat3")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float*, NativeBool32> NativeInputFloat3;
    [NativeFunction("UIInputFloat4")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float*, NativeBool32> NativeInputFloat4;
    [NativeFunction("UIInputInt")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, int*, int, int, NativeBool32> NativeInputInt;
    [NativeFunction("UIDragFloat")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float*, float, float, float, NativeBool32> NativeDragFloat;
    [NativeFunction("UIDragFloat3")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float*, float, float, float, NativeBool32> NativeDragFloat3;
    [NativeFunction("UISliderFloat")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float*, float, float, NativeBool32> NativeSliderFloat;
    [NativeFunction("UISliderInt")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, int*, int, int, NativeBool32> NativeSliderInt;
    [NativeFunction("UIColorEdit3")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float*, NativeBool32> NativeColorEdit3;
    [NativeFunction("UIColorEdit4")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float*, NativeBool32> NativeColorEdit4;
    [NativeFunction("UITreeNode")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, NativeBool32> NativeTreeNode;
    [NativeFunction("UITreeNodeEx")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, int, NativeBool32> NativeTreeNodeEx;
    [NativeFunction("UITreePop")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeTreePop;
    [NativeFunction("UICollapsingHeader")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, int, NativeBool32> NativeCollapsingHeader;
    [NativeFunction("UISelectable")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, NativeBool32, int, NativeBool32> NativeSelectable;
    [NativeFunction("UIBeginCombo")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, byte*, int, NativeBool32> NativeBeginCombo;
    [NativeFunction("UIEndCombo")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeEndCombo;
    [NativeFunction("UIBeginListBox")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float, float, NativeBool32> NativeBeginListBox;
    [NativeFunction("UIEndListBox")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeEndListBox;
    [NativeFunction("UIBeginTabBar")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, int, NativeBool32> NativeBeginTabBar;
    [NativeFunction("UIEndTabBar")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeEndTabBar;
    [NativeFunction("UIBeginTabItem")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, int, NativeBool32> NativeBeginTabItem;
    [NativeFunction("UIEndTabItem")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeEndTabItem;
    [NativeFunction("UIBeginMenuBar")]
    internal static unsafe delegate* unmanaged[Cdecl]<NativeBool32> NativeBeginMenuBar;
    [NativeFunction("UIEndMenuBar")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeEndMenuBar;
    [NativeFunction("UIBeginMainMenuBar")]
    internal static unsafe delegate* unmanaged[Cdecl]<NativeBool32> NativeBeginMainMenuBar;
    [NativeFunction("UIEndMainMenuBar")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeEndMainMenuBar;
    [NativeFunction("UIBeginMenu")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, NativeBool32, NativeBool32> NativeBeginMenu;
    [NativeFunction("UIEndMenu")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeEndMenu;
    [NativeFunction("UIMenuItem")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, byte*, NativeBool32, NativeBool32, NativeBool32> NativeMenuItem;
    [NativeFunction("UIOpenPopup")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, void> NativeOpenPopup;
    [NativeFunction("UIBeginPopup")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, int, NativeBool32> NativeBeginPopup;
    [NativeFunction("UIBeginPopupContextItem")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, int, NativeBool32> NativeBeginPopupContextItem;
    [NativeFunction("UIBeginPopupContextWindow")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, int, NativeBool32> NativeBeginPopupContextWindow;
    [NativeFunction("UIInputTextWithHint")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, byte*, byte*, int, int, NativeBool32> NativeInputTextWithHint;
    [NativeFunction("UIIsItemToggledOpen")]
    internal static unsafe delegate* unmanaged[Cdecl]<NativeBool32> NativeIsItemToggledOpen;
    [NativeFunction("UIWantTextInput")]
    internal static unsafe delegate* unmanaged[Cdecl]<NativeBool32> NativeWantTextInput;
    [NativeFunction("UIShortcut")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, int, NativeBool32, NativeBool32> NativeShortcut;
    [NativeFunction("UIBeginPopupModal")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, int, NativeBool32> NativeBeginPopupModal;
    [NativeFunction("UIEndPopup")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeEndPopup;
    [NativeFunction("UICloseCurrentPopup")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeCloseCurrentPopup;
    [NativeFunction("UIBeginTable")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, int, int, NativeBool32> NativeBeginTable;
    [NativeFunction("UIEndTable")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeEndTable;
    [NativeFunction("UITableNextRow")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, float, void> NativeTableNextRow;
    [NativeFunction("UITableNextColumn")]
    internal static unsafe delegate* unmanaged[Cdecl]<NativeBool32> NativeTableNextColumn;
    [NativeFunction("UITableSetColumnIndex")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, NativeBool32> NativeTableSetColumnIndex;
    [NativeFunction("UITableSetupColumn")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, int, float, void> NativeTableSetupColumn;
    [NativeFunction("UITableHeadersRow")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeTableHeadersRow;
    [NativeFunction("UIGetContentRegionAvail")]
    internal static unsafe delegate* unmanaged[Cdecl]<float*, float*, void> NativeGetContentRegionAvail;
    [NativeFunction("UIGetWindowSize")]
    internal static unsafe delegate* unmanaged[Cdecl]<float*, float*, void> NativeGetWindowSize;
    [NativeFunction("UIGetWindowPos")]
    internal static unsafe delegate* unmanaged[Cdecl]<float*, float*, void> NativeGetWindowPos;
    [NativeFunction("UISetNextWindowSize")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, float, int, void> NativeSetNextWindowSize;
    [NativeFunction("UISetNextWindowPos")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, float, int, void> NativeSetNextWindowPos;
    [NativeFunction("UIIsItemHovered")]
    internal static unsafe delegate* unmanaged[Cdecl]<NativeBool32> NativeIsItemHovered;
    [NativeFunction("UIIsItemClicked")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, NativeBool32> NativeIsItemClicked;
    [NativeFunction("UIIsItemActive")]
    internal static unsafe delegate* unmanaged[Cdecl]<NativeBool32> NativeIsItemActive;
    [NativeFunction("UIIsWindowFocused")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, NativeBool32> NativeIsWindowFocused;
    [NativeFunction("UIIsWindowHovered")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, NativeBool32> NativeIsWindowHovered;
    [NativeFunction("UIPushStyleColor")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, float, float, float, float, void> NativePushStyleColor;
    [NativeFunction("UIPopStyleColor")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, void> NativePopStyleColor;
    [NativeFunction("UIPushStyleVarFloat")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, float, void> NativePushStyleVarFloat;
    [NativeFunction("UIPushStyleVarVec2")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, float, float, void> NativePushStyleVarVec2;
    [NativeFunction("UIPopStyleVar")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, void> NativePopStyleVar;
    [NativeFunction("UIPushItemWidth")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, void> NativePushItemWidth;
    [NativeFunction("UIPopItemWidth")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativePopItemWidth;
    [NativeFunction("UIPushIDStr")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, void> NativePushIDStr;
    [NativeFunction("UIPushIDInt")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, void> NativePushIDInt;
    [NativeFunction("UIPopID")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativePopID;
    [NativeFunction("UISetTooltip")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, void> NativeSetTooltip;
    [NativeFunction("UIBeginTooltip")]
    internal static unsafe delegate* unmanaged[Cdecl]<NativeBool32> NativeBeginTooltip;
    [NativeFunction("UIEndTooltip")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeEndTooltip;
    [NativeFunction("UIBeginDragDropSource")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, NativeBool32> NativeBeginDragDropSource;
    [NativeFunction("UISetDragDropPayload")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, IntPtr, int, NativeBool32> NativeSetDragDropPayload;
    [NativeFunction("UIEndDragDropSource")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeEndDragDropSource;
    [NativeFunction("UIBeginDragDropTarget")]
    internal static unsafe delegate* unmanaged[Cdecl]<NativeBool32> NativeBeginDragDropTarget;
    [NativeFunction("UIEndDragDropTarget")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeEndDragDropTarget;
    [NativeFunction("UISeparatorText")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, void> NativeSeparatorText;
    [NativeFunction("UITextDisabled")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, void> NativeTextDisabled;
    [NativeFunction("UIBullet")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeBullet;
    [NativeFunction("UIBeginDisabled")]
    internal static unsafe delegate* unmanaged[Cdecl]<NativeBool32, void> NativeBeginDisabled;
    [NativeFunction("UIEndDisabled")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeEndDisabled;
    [NativeFunction("UISetClipboardText")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, void> NativeSetClipboardText;
    [NativeFunction("UIGetClipboardText")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*> NativeGetClipboardText;
    [NativeFunction("UIBeginGroup")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeBeginGroup;
    [NativeFunction("UIEndGroup")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeEndGroup;
    [NativeFunction("UISetNextItemWidth")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, void> NativeSetNextItemWidth;
    [NativeFunction("UIAlignTextToFramePadding")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeAlignTextToFramePadding;
    [NativeFunction("UIGetCursorPos")]
    internal static unsafe delegate* unmanaged[Cdecl]<float*, float*, void> NativeGetCursorPos;
    [NativeFunction("UISetCursorPos")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, float, void> NativeSetCursorPos;
    [NativeFunction("UIGetCursorScreenPos")]
    internal static unsafe delegate* unmanaged[Cdecl]<float*, float*, void> NativeGetCursorScreenPos;
    [NativeFunction("UISetCursorScreenPos")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, float, void> NativeSetCursorScreenPos;
    [NativeFunction("UIGetScrollX")]
    internal static unsafe delegate* unmanaged[Cdecl]<float> NativeGetScrollX;
    [NativeFunction("UIGetScrollY")]
    internal static unsafe delegate* unmanaged[Cdecl]<float> NativeGetScrollY;
    [NativeFunction("UISetScrollX")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, void> NativeSetScrollX;
    [NativeFunction("UISetScrollY")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, void> NativeSetScrollY;
    [NativeFunction("UIGetScrollMaxX")]
    internal static unsafe delegate* unmanaged[Cdecl]<float> NativeGetScrollMaxX;
    [NativeFunction("UIGetScrollMaxY")]
    internal static unsafe delegate* unmanaged[Cdecl]<float> NativeGetScrollMaxY;
    [NativeFunction("UISetScrollHereY")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, void> NativeSetScrollHereY;
    [NativeFunction("UISetNextItemOpen")]
    internal static unsafe delegate* unmanaged[Cdecl]<NativeBool32, int, void> NativeSetNextItemOpen;
    [NativeFunction("UISetKeyboardFocusHere")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, void> NativeSetKeyboardFocusHere;
    [NativeFunction("UIIsItemDeactivatedAfterEdit")]
    internal static unsafe delegate* unmanaged[Cdecl]<NativeBool32> NativeIsItemDeactivatedAfterEdit;
    [NativeFunction("UIIsWindowAppearing")]
    internal static unsafe delegate* unmanaged[Cdecl]<NativeBool32> NativeIsWindowAppearing;
    [NativeFunction("UIGetMousePos")]
    internal static unsafe delegate* unmanaged[Cdecl]<float*, float*, void> NativeGetMousePos;
    [NativeFunction("UIIsMouseDown")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, NativeBool32> NativeIsMouseDown;
    [NativeFunction("UIIsMouseClicked")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, NativeBool32> NativeIsMouseClicked;
    [NativeFunction("UIIsMouseDoubleClicked")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, NativeBool32> NativeIsMouseDoubleClicked;
    [NativeFunction("UIIsMouseReleased")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, NativeBool32> NativeIsMouseReleased;
    [NativeFunction("UIGetMouseDragDelta")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, float, float*, float*, void> NativeGetMouseDragDelta;
    [NativeFunction("UIGetMouseWheel")]
    internal static unsafe delegate* unmanaged[Cdecl]<float*, float*, void> NativeGetMouseWheel;
    [NativeFunction("UIIsMouseHoveringRect")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, float, float, float, NativeBool32, NativeBool32> NativeIsMouseHoveringRect;
    [NativeFunction("UIIsKeyDown")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, NativeBool32> NativeIsKeyDown;
    [NativeFunction("UIIsKeyPressed")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, NativeBool32, NativeBool32> NativeIsKeyPressed;
    [NativeFunction("UIGetKeyMods")]
    internal static unsafe delegate* unmanaged[Cdecl]<int> NativeGetKeyMods;
    [NativeFunction("UIGetItemRectMin")]
    internal static unsafe delegate* unmanaged[Cdecl]<float*, float*, void> NativeGetItemRectMin;
    [NativeFunction("UIGetItemRectMax")]
    internal static unsafe delegate* unmanaged[Cdecl]<float*, float*, void> NativeGetItemRectMax;
    [NativeFunction("UIGetItemRectSize")]
    internal static unsafe delegate* unmanaged[Cdecl]<float*, float*, void> NativeGetItemRectSize;
    [NativeFunction("UIDragInt")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, int*, float, int, int, NativeBool32> NativeDragInt;
    [NativeFunction("UIDragFloat2")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float*, float, float, float, NativeBool32> NativeDragFloat2;
    [NativeFunction("UIDragFloat4")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float*, float, float, float, NativeBool32> NativeDragFloat4;
    [NativeFunction("UISliderFloat2")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float*, float, float, NativeBool32> NativeSliderFloat2;
    [NativeFunction("UISliderFloat3")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float*, float, float, NativeBool32> NativeSliderFloat3;
    [NativeFunction("UISliderFloat4")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float*, float, float, NativeBool32> NativeSliderFloat4;
    [NativeFunction("UIAcceptDragDropPayload")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, int, int*, void*> NativeAcceptDragDropPayload;

    /// a window made this way is nobody's but the caller's: it is not in the View menu,
    ///  cannot be opened or closed by name, and is drawn whenever this is called. declare a
    ///  window with [EditorWindow] on a UIWindow instead, unless that is what is wanted
    public static bool BeginWindow(string title, WindowFlags flags = WindowFlags.None)
    {
      bool open = false;
      unsafe
      {
        using var title_utf8 = new Utf8Arg(title, stackalloc byte[Utf8Arg.StackSize]);
        NativeBool32 result = NativeBeginWindow(title_utf8.Pointer, (int)flags);
        open = result;
      }
      return open;
    }

    public static void EndWindow()
    {
      unsafe
      {
        NativeEndWindow();
      }
    }

    public static bool BeginChild(string name, WindowFlags flags)
    {
      return BeginChild(name, 0, 0, ChildFlags.None, flags);
    }

    /// a size of 0 on an axis takes what is left of it; the two sets of flags are ImGui's own
    public static bool BeginChild(string name, float width, float height, ChildFlags child_flags = ChildFlags.None, WindowFlags window_flags = WindowFlags.None)
    {
      unsafe
      {
        using var name_utf8 = new Utf8Arg(name, stackalloc byte[Utf8Arg.StackSize]);
        return NativeBeginChild(name_utf8.Pointer, width, height, (int)child_flags, (int)window_flags);
      }
    }

    public static void EndChild()
    {
      unsafe
      {
        NativeEndChild();
      }
    }

    public static void Text(string text)
    {
      unsafe
      {
        using var text_utf8 = new Utf8Arg(text, stackalloc byte[Utf8Arg.StackSize]);
        NativeText(text_utf8.Pointer);
      }
    }

    public static void TextColored(float r, float g, float b, float a, string text)
    {
      unsafe { 
        using var text_utf8 = new Utf8Arg(text, stackalloc byte[Utf8Arg.StackSize]);
        NativeTextColored(r, g, b, a, text_utf8.Pointer); 
      }
    }

    public static void TextColored(Vec4 color, string text)
    {
      unsafe { 
        using var text_utf8 = new Utf8Arg(text, stackalloc byte[Utf8Arg.StackSize]);
        NativeTextColored(color.X, color.Y, color.Z, color.W, text_utf8.Pointer); 
      }
    }

    public static void TextWrapped(string text)
    {
      unsafe { 
        using var text_utf8 = new Utf8Arg(text, stackalloc byte[Utf8Arg.StackSize]);
        NativeTextWrapped(text_utf8.Pointer); 
      }
    }

    public static void LabelText(string label, string text)
    {
      unsafe { 
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        using var text_utf8 = new Utf8Arg(text, stackalloc byte[Utf8Arg.StackSize]);
        NativeLabelText(label_utf8.Pointer, text_utf8.Pointer); 
      }
    }

    public static bool Button(string label, float width = 0, float height = 0)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        return NativeButton(label_utf8.Pointer, width, height);
      }
    }

    public static bool SmallButton(string label)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        return NativeSmallButton(label_utf8.Pointer);
      }
    }

    public static bool InvisibleButton(string str_id, float width, float height)
    {
      unsafe
      {
        using var str_id_utf8 = new Utf8Arg(str_id, stackalloc byte[Utf8Arg.StackSize]);
        return NativeInvisibleButton(str_id_utf8.Pointer, width, height);
      }
    }

    public static bool Checkbox(string label, ref bool value)
    {
      unsafe
      {
        NativeBool32 nb = value;
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        bool changed = NativeCheckbox(label_utf8.Pointer, &nb);
        value = nb;
        return changed;
      }
    }

    public static bool RadioButton(string label, bool active)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        return NativeRadioButton(label_utf8.Pointer, active);
      }
    }

    public static void ProgressBar(float fraction, float width = -1, float height = 0, string overlay = "")
    {
      unsafe
      {
        using var overlay_utf8 = new Utf8Arg(overlay, stackalloc byte[Utf8Arg.StackSize]);
        NativeProgressBar(fraction, width, height, overlay_utf8.Pointer);
      }
    }

    public static void Separator() { unsafe { NativeSeparator(); } }
    public static void SameLine(float offset = 0, float spacing = -1) { unsafe { NativeSameLine(offset, spacing); } }
    public static void Spacing() { unsafe { NativeSpacing(); } }
    public static void Indent(float width = 0) { unsafe { NativeIndent(width); } }
    public static void Unindent(float width = 0) { unsafe { NativeUnindent(width); } }
    public static void NewLine() { unsafe { NativeNewLine(); } }
    public static void Dummy(float width, float height) { unsafe { NativeDummy(width, height); } }

    /// a text field that shows hint, greyed, while it is empty. as InputText otherwise
    public static bool InputTextWithHint(string label, string hint, ref string text, int max_length, InputTextFlags flags = InputTextFlags.None)
    {
      byte[] buffer = ArrayPool<byte>.Shared.Rent(max_length + 1);
      try
      {
        Span<byte> edited = buffer.AsSpan(0, max_length + 1);
        Utf8Arg.Fill(text, edited);
        unsafe
        {
          using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
          using var hint_utf8 = new Utf8Arg(hint, stackalloc byte[Utf8Arg.StackSize]);
          fixed (byte* ptr = edited)
          {
            bool changed = NativeInputTextWithHint(label_utf8.Pointer, hint_utf8.Pointer, ptr, edited.Length, (int)flags);
            if (changed)
            {
              text = Utf8Arg.Read(edited);
            }
            return changed;
          }
        }
      }
      finally
      {
        ArrayPool<byte>.Shared.Return(buffer);
      }
    }

    /// max_length is in bytes of UTF-8. true when the text was edited this frame
    public static bool InputText(string label, ref string text, int max_length, InputTextFlags flags = InputTextFlags.None)
    {
      byte[] buffer = ArrayPool<byte>.Shared.Rent(max_length + 1);
      try
      {
        Span<byte> edited = buffer.AsSpan(0, max_length + 1);
        Utf8Arg.Fill(text, edited);
        unsafe
        {
          using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
          fixed (byte* ptr = edited)
          {
            bool changed = NativeInputText(label_utf8.Pointer, ptr, edited.Length, (int)flags);
            if (changed)
            {
              text = Utf8Arg.Read(edited);
            }
            return changed;
          }
        }
      }
      finally
      {
        ArrayPool<byte>.Shared.Return(buffer);
      }
    }

    /// max_length is in bytes of UTF-8. true when the text was edited this frame
    public static bool InputTextMultiline(string label, ref string text, int max_length, float width = 0, float height = 0, InputTextFlags flags = InputTextFlags.None)
    {
      byte[] buffer = ArrayPool<byte>.Shared.Rent(max_length + 1);
      try
      {
        Span<byte> edited = buffer.AsSpan(0, max_length + 1);
        Utf8Arg.Fill(text, edited);
        unsafe
        {
          using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
          fixed (byte* ptr = edited)
          {
            bool changed = NativeInputTextMultiline(label_utf8.Pointer, ptr, edited.Length, width, height, (int)flags);
            if (changed)
            {
              text = Utf8Arg.Read(edited);
            }
            return changed;
          }
        }
      }
      finally
      {
        ArrayPool<byte>.Shared.Return(buffer);
      }
    }

    public static bool InputFloat(string label, ref float value, float step = 0, float step_fast = 0, int decimal_precision = 3)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        fixed (float* ptr = &value)
          return NativeInputFloat(label_utf8.Pointer, ptr, step, step_fast, decimal_precision);
      }
    }

    public static bool InputFloat2(string label, float[] values)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        fixed (float* ptr = values)
          return NativeInputFloat2(label_utf8.Pointer, ptr);
      }
    }

    public static bool InputFloat3(string label, float[] values)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        fixed (float* ptr = values)
          return NativeInputFloat3(label_utf8.Pointer, ptr);
      }
    }

    public static bool InputFloat3(string label, ref Vec3 value)
    {
      float[] arr = new float[] { value.X, value.Y, value.Z };
      unsafe
      {
        fixed (float* ptr = arr)
        {
          using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
          bool changed = NativeInputFloat3(label_utf8.Pointer, ptr);
          if (changed) value = new Vec3(arr[0], arr[1], arr[2]);
          return changed;
        }
      }
    }

    public static bool InputFloat4(string label, float[] values)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        fixed (float* ptr = values)
          return NativeInputFloat4(label_utf8.Pointer, ptr);
      }
    }

    public static bool InputInt(string label, ref int value, int step = 1, int step_fast = 100)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        fixed (int* ptr = &value)
          return NativeInputInt(label_utf8.Pointer, ptr, step, step_fast);
      }
    }

    public static bool DragFloat(string label, ref float value, float speed = 1.0f, float min = 0, float max = 0)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        fixed (float* ptr = &value)
          return NativeDragFloat(label_utf8.Pointer, ptr, speed, min, max);
      }
    }

    public static bool DragFloat3(string label, ref Vec3 value, float speed = 1.0f, float min = 0, float max = 0)
    {
      float[] arr = new float[] { value.X, value.Y, value.Z };
      unsafe
      {
        fixed (float* ptr = arr)
        {
          using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
          bool changed = NativeDragFloat3(label_utf8.Pointer, ptr, speed, min, max);
          if (changed) value = new Vec3(arr[0], arr[1], arr[2]);
          return changed;
        }
      }
    }

    public static bool SliderFloat(string label, ref float value, float min, float max)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        fixed (float* ptr = &value)
          return NativeSliderFloat(label_utf8.Pointer, ptr, min, max);
      }
    }

    public static bool SliderInt(string label, ref int value, int min, int max)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        fixed (int* ptr = &value)
          return NativeSliderInt(label_utf8.Pointer, ptr, min, max);
      }
    }

    public static bool ColorEdit3(string label, ref Vec3 color)
    {
      float[] arr = new float[] { color.X, color.Y, color.Z };
      unsafe
      {
        fixed (float* ptr = arr)
        {
          using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
          bool changed = NativeColorEdit3(label_utf8.Pointer, ptr);
          if (changed) color = new Vec3(arr[0], arr[1], arr[2]);
          return changed;
        }
      }
    }

    public static bool ColorEdit4(string label, ref Vec4 color)
    {
      float[] arr = new float[] { color.X, color.Y, color.Z, color.W };
      unsafe
      {
        fixed (float* ptr = arr)
        {
          using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
          bool changed = NativeColorEdit4(label_utf8.Pointer, ptr);
          if (changed) color = new Vec4(arr[0], arr[1], arr[2], arr[3]);
          return changed;
        }
      }
    }

    public static bool TreeNode(string label)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        return NativeTreeNode(label_utf8.Pointer);
      }
    }
    public static bool TreeNodeEx(string label, TreeNodeFlags flags = TreeNodeFlags.None)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        return NativeTreeNodeEx(label_utf8.Pointer, (int)flags);
      }
    }
    public static void TreePop() { unsafe { NativeTreePop(); } }
    public static bool CollapsingHeader(string label, TreeNodeFlags flags = TreeNodeFlags.None)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        return NativeCollapsingHeader(label_utf8.Pointer, (int)flags);
      }
    }

    public static bool Selectable(string label, bool selected = false, int flags = 0)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        return NativeSelectable(label_utf8.Pointer, selected, flags);
      }
    }
    public static bool BeginCombo(string label, string preview_value, int flags = 0)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        using var preview_value_utf8 = new Utf8Arg(preview_value, stackalloc byte[Utf8Arg.StackSize]);
        return NativeBeginCombo(label_utf8.Pointer, preview_value_utf8.Pointer, flags);
      }
    }
    public static void EndCombo() { unsafe { NativeEndCombo(); } }
    public static bool BeginListBox(string label, float width = 0, float height = 0)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        return NativeBeginListBox(label_utf8.Pointer, width, height);
      }
    }
    public static void EndListBox() { unsafe { NativeEndListBox(); } }

    public static bool BeginTabBar(string str_id, int flags = 0)
    {
      unsafe
      {
        using var str_id_utf8 = new Utf8Arg(str_id, stackalloc byte[Utf8Arg.StackSize]);
        return NativeBeginTabBar(str_id_utf8.Pointer, flags);
      }
    }
    public static void EndTabBar() { unsafe { NativeEndTabBar(); } }
    public static bool BeginTabItem(string label, int flags = 0)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        return NativeBeginTabItem(label_utf8.Pointer, flags);
      }
    }
    public static void EndTabItem() { unsafe { NativeEndTabItem(); } }

    public static bool BeginMenuBar() { unsafe { return NativeBeginMenuBar(); } }
    public static void EndMenuBar() { unsafe { NativeEndMenuBar(); } }
    public static bool BeginMainMenuBar() { unsafe { return NativeBeginMainMenuBar(); } }
    public static void EndMainMenuBar() { unsafe { NativeEndMainMenuBar(); } }
    public static bool BeginMenu(string label, bool enabled = true)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        return NativeBeginMenu(label_utf8.Pointer, enabled);
      }
    }
    public static void EndMenu() { unsafe { NativeEndMenu(); } }
    public static bool MenuItem(string label, string shortcut = "", bool selected = false, bool enabled = true)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        using var shortcut_utf8 = new Utf8Arg(shortcut, stackalloc byte[Utf8Arg.StackSize]);
        return NativeMenuItem(label_utf8.Pointer, shortcut_utf8.Pointer, selected, enabled);
      }
    }

    public static void OpenPopup(string str_id)
    {
      unsafe
      {
        using var str_id_utf8 = new Utf8Arg(str_id, stackalloc byte[Utf8Arg.StackSize]);
        NativeOpenPopup(str_id_utf8.Pointer);
      }
    }
    public static bool BeginPopup(string str_id, int flags = 0)
    {
      unsafe
      {
        using var str_id_utf8 = new Utf8Arg(str_id, stackalloc byte[Utf8Arg.StackSize]);
        return NativeBeginPopup(str_id_utf8.Pointer, flags);
      }
    }
    /// a popup that the item just drawn opens when it is clicked with the right mouse
    ///  button: drawn like BeginPopup, ended with EndPopup. with no id it is the item's own
    public static bool BeginPopupContextItem(string? str_id = null, PopupFlags flags = PopupFlags.MouseButtonRight)
    {
      unsafe
      {
        using var str_id_utf8 = new Utf8Arg(str_id ?? "", stackalloc byte[Utf8Arg.StackSize]);
        return NativeBeginPopupContextItem(str_id_utf8.Pointer, (int)flags);
      }
    }

    /// the same for a click on the window where nothing else takes it
    public static bool BeginPopupContextWindow(string? str_id = null, PopupFlags flags = PopupFlags.MouseButtonRight)
    {
      unsafe
      {
        using var str_id_utf8 = new Utf8Arg(str_id ?? "", stackalloc byte[Utf8Arg.StackSize]);
        return NativeBeginPopupContextWindow(str_id_utf8.Pointer, (int)flags);
      }
    }

    public static bool BeginPopupModal(string name, int flags = 0)
    {
      unsafe
      {
        using var name_utf8 = new Utf8Arg(name, stackalloc byte[Utf8Arg.StackSize]);
        return NativeBeginPopupModal(name_utf8.Pointer, flags);
      }
    }
    public static void EndPopup() { unsafe { NativeEndPopup(); } }
    public static void CloseCurrentPopup() { unsafe { NativeCloseCurrentPopup(); } }

    public static bool BeginTable(string str_id, int columns, TableFlags flags = TableFlags.None)
    {
      unsafe
      {
        using var str_id_utf8 = new Utf8Arg(str_id, stackalloc byte[Utf8Arg.StackSize]);
        return NativeBeginTable(str_id_utf8.Pointer, columns, (int)flags);
      }
    }
    public static void EndTable() { unsafe { NativeEndTable(); } }
    public static void TableNextRow(int flags = 0, float min_row_height = 0) { unsafe { NativeTableNextRow(flags, min_row_height); } }
    public static bool TableNextColumn() { unsafe { return NativeTableNextColumn(); } }
    public static bool TableSetColumnIndex(int column_n) { unsafe { return NativeTableSetColumnIndex(column_n); } }
    public static void TableSetupColumn(string label, int flags = 0, float init_width = 0)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        NativeTableSetupColumn(label_utf8.Pointer, flags, init_width);
      }
    }
    public static void TableHeadersRow() { unsafe { NativeTableHeadersRow(); } }

    public static Vec2 GetContentRegionAvail()
    {
      float x = 0, y = 0;
      unsafe { NativeGetContentRegionAvail(&x, &y); }
      return new Vec2(x, y);
    }

    public static Vec2 GetWindowSize()
    {
      float w = 0, h = 0;
      unsafe { NativeGetWindowSize(&w, &h); }
      return new Vec2(w, h);
    }

    public static Vec2 GetWindowPos()
    {
      float x = 0, y = 0;
      unsafe { NativeGetWindowPos(&x, &y); }
      return new Vec2(x, y);
    }

    public static void SetNextWindowSize(float width, float height, Condition cond = Condition.None) { unsafe { NativeSetNextWindowSize(width, height, (int)cond); } }
    public static void SetNextWindowSize(Vec2 size, Condition cond = Condition.None) { unsafe { NativeSetNextWindowSize(size.X, size.Y, (int)cond); } }
    public static void SetNextWindowPos(float x, float y, Condition cond = Condition.None) { unsafe { NativeSetNextWindowPos(x, y, (int)cond); } }
    public static void SetNextWindowPos(Vec2 pos, Condition cond = Condition.None) { unsafe { NativeSetNextWindowPos(pos.X, pos.Y, (int)cond); } }
    public static bool IsItemHovered() { unsafe { return NativeIsItemHovered(); } }
    public static bool IsItemClicked(MouseButton mouse_button = MouseButton.Left) { unsafe { return NativeIsItemClicked((int)mouse_button); } }
    public static bool IsItemActive() { unsafe { return NativeIsItemActive(); } }
    /// the last item is a tree node whose arrow was clicked this frame, so that a click on
    ///  the arrow can be told from a click on the row
    public static bool IsItemToggledOpen() { unsafe { return NativeIsItemToggledOpen(); } }
    /// a text field is being typed in: a key pressed now is its, and not a shortcut's
    public static bool WantTextInput { get { unsafe { return NativeWantTextInput(); } } }
    public static bool IsWindowFocused(int flags = 0) { unsafe { return NativeIsWindowFocused(flags); } }
    public static bool IsWindowHovered(int flags = 0) { unsafe { return NativeIsWindowHovered(flags); } }

    public static void PushStyleColor(StyleColor idx, Vec4 color) { unsafe { NativePushStyleColor((int)idx, color.X, color.Y, color.Z, color.W); } }
    public static void PushStyleColor(StyleColor idx, float r, float g, float b, float a) { unsafe { NativePushStyleColor((int)idx, r, g, b, a); } }
    public static void PopStyleColor(int count = 1) { unsafe { NativePopStyleColor(count); } }
    public static void PushStyleVar(StyleVar idx, float val) { unsafe { NativePushStyleVarFloat((int)idx, val); } }
    public static void PushStyleVar(StyleVar idx, Vec2 val) { unsafe { NativePushStyleVarVec2((int)idx, val.X, val.Y); } }
    public static void PopStyleVar(int count = 1) { unsafe { NativePopStyleVar(count); } }
    public static void PushItemWidth(float width) { unsafe { NativePushItemWidth(width); } }
    public static void PopItemWidth() { unsafe { NativePopItemWidth(); } }

    public static void PushID(string str_id)
    {
      unsafe
      {
        using var str_id_utf8 = new Utf8Arg(str_id, stackalloc byte[Utf8Arg.StackSize]);
        NativePushIDStr(str_id_utf8.Pointer);
      }
    }
    public static void PushID(int int_id) { unsafe { NativePushIDInt(int_id); } }
    public static void PopID() { unsafe { NativePopID(); } }

    public static void SetTooltip(string text)
    {
      unsafe
      {
        using var text_utf8 = new Utf8Arg(text, stackalloc byte[Utf8Arg.StackSize]);
        NativeSetTooltip(text_utf8.Pointer);
      }
    }
    public static bool BeginTooltip() { unsafe { return NativeBeginTooltip(); } }
    public static void EndTooltip() { unsafe { NativeEndTooltip(); } }

    public static bool BeginDragDropSource(int flags = 0) { unsafe { return NativeBeginDragDropSource(flags); } }

    /// type names what is carried, so a target can tell whether it wants it; ImGui copies the bytes
    public static bool SetDragDropPayload(string type, IntPtr data, int size)
    {
      unsafe
      {
        using var type_utf8 = new Utf8Arg(type, stackalloc byte[Utf8Arg.StackSize]);
        return NativeSetDragDropPayload(type_utf8.Pointer, data, size);
      }
    }
    public static void EndDragDropSource() { unsafe { NativeEndDragDropSource(); } }
    public static bool BeginDragDropTarget() { unsafe { return NativeBeginDragDropTarget(); } }
    public static void EndDragDropTarget() { unsafe { NativeEndDragDropTarget(); } }

    /// a separator with a label in it: the header of a section
    public static void SeparatorText(string label)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        NativeSeparatorText(label_utf8.Pointer);
      }
    }

    public static void TextDisabled(string text)
    {
      unsafe
      {
        using var text_utf8 = new Utf8Arg(text, stackalloc byte[Utf8Arg.StackSize]);
        NativeTextDisabled(text_utf8.Pointer);
      }
    }

    /// a bullet, with what is drawn next on the same line
    public static void Bullet() { unsafe { NativeBullet(); } }

    /// what is drawn until EndDisabled is greyed and takes no input when disabled is true
    public static void BeginDisabled(bool disabled = true) { unsafe { NativeBeginDisabled(disabled); } }
    public static void EndDisabled() { unsafe { NativeEndDisabled(); } }

    public static string ClipboardText
    {
      get
      {
        unsafe { return System.Runtime.InteropServices.Marshal.PtrToStringUTF8((IntPtr)NativeGetClipboardText()) ?? string.Empty; }
      }
      set
      {
        unsafe
        {
          using var text_utf8 = new Utf8Arg(value, stackalloc byte[Utf8Arg.StackSize]);
          NativeSetClipboardText(text_utf8.Pointer);
        }
      }
    }

    /// what is drawn until EndGroup is one item to what comes after: SameLine and the item
    ///  rectangle treat it as one
    public static void BeginGroup() { unsafe { NativeBeginGroup(); } }
    public static void EndGroup() { unsafe { NativeEndGroup(); } }

    /// the width of the next item only. a negative width is that far from the right edge
    public static void SetNextItemWidth(float width) { unsafe { NativeSetNextItemWidth(width); } }
    /// text drawn next sits on the baseline of the framed items after it on the line
    public static void AlignTextToFramePadding() { unsafe { NativeAlignTextToFramePadding(); } }

    /// where the next item goes, from the window's top left corner
    public static Vec2 CursorPos
    {
      get
      {
        float x, y;
        unsafe { NativeGetCursorPos(&x, &y); }
        return new Vec2(x, y);
      }
      set { unsafe { NativeSetCursorPos(value.X, value.Y); } }
    }

    /// the same place on the screen, which is what the draw list and the mouse use
    public static Vec2 CursorScreenPos
    {
      get
      {
        float x, y;
        unsafe { NativeGetCursorScreenPos(&x, &y); }
        return new Vec2(x, y);
      }
      set { unsafe { NativeSetCursorScreenPos(value.X, value.Y); } }
    }

    public static float ScrollX
    {
      get { unsafe { return NativeGetScrollX(); } }
      set { unsafe { NativeSetScrollX(value); } }
    }

    public static float ScrollY
    {
      get { unsafe { return NativeGetScrollY(); } }
      set { unsafe { NativeSetScrollY(value); } }
    }

    public static float ScrollMaxX { get { unsafe { return NativeGetScrollMaxX(); } } }
    public static float ScrollMaxY { get { unsafe { return NativeGetScrollMaxY(); } } }

    /// scrolls so the place the next item goes is in view: 0 at the top, 0.5 in the middle,
    ///  1 at the bottom
    public static void SetScrollHereY(float center_ratio = 0.5f) { unsafe { NativeSetScrollHereY(center_ratio); } }

    /// whether the next tree node or collapsing header is open
    public static void SetNextItemOpen(bool open, Condition cond = Condition.None) { unsafe { NativeSetNextItemOpen(open, (int)cond); } }
    /// gives the keyboard to the next item, or with an offset to one after it
    public static void SetKeyboardFocusHere(int offset = 0) { unsafe { NativeSetKeyboardFocusHere(offset); } }
    /// the last item was being edited, is not any more, and its value changed: the moment
    ///  to take an edit as finished
    public static bool IsItemDeactivatedAfterEdit() { unsafe { return NativeIsItemDeactivatedAfterEdit(); } }
    /// the window is shown this frame and was not the frame before
    public static bool IsWindowAppearing() { unsafe { return NativeIsWindowAppearing(); } }

    /// the mouse as the UI has it, on the screen
    public static Vec2 MousePos
    {
      get
      {
        float x, y;
        unsafe { NativeGetMousePos(&x, &y); }
        return new Vec2(x, y);
      }
    }

    public static bool IsMouseDown(MouseButton button = MouseButton.Left) { unsafe { return NativeIsMouseDown((int)button); } }
    public static bool IsMouseClicked(MouseButton button = MouseButton.Left) { unsafe { return NativeIsMouseClicked((int)button); } }
    public static bool IsMouseDoubleClicked(MouseButton button = MouseButton.Left) { unsafe { return NativeIsMouseDoubleClicked((int)button); } }
    public static bool IsMouseReleased(MouseButton button = MouseButton.Left) { unsafe { return NativeIsMouseReleased((int)button); } }

    /// how far the mouse has moved since the button went down; zero until it has moved
    ///  further than the threshold. a negative threshold is ImGui's own
    public static Vec2 GetMouseDragDelta(MouseButton button = MouseButton.Left, float lock_threshold = -1f)
    {
      float x, y;
      unsafe { NativeGetMouseDragDelta((int)button, lock_threshold, &x, &y); }
      return new Vec2(x, y);
    }

    /// this frame's wheel: X sideways, Y up and down
    public static Vec2 MouseWheel
    {
      get
      {
        float x, y;
        unsafe { NativeGetMouseWheel(&x, &y); }
        return new Vec2(x, y);
      }
    }

    /// the mouse is over that rectangle of the screen. clip cuts the rectangle to what the
    ///  window is showing
    public static bool IsMouseHoveringRect(Vec2 min, Vec2 max, bool clip = true) { unsafe { return NativeIsMouseHoveringRect(min.X, min.Y, max.X, max.Y, clip); } }

    public static bool IsKeyDown(Key key) { unsafe { return NativeIsKeyDown((int)key); } }
    /// true on the frame the key goes down, and again as it repeats when repeat is true
    public static bool IsKeyPressed(Key key, bool repeat = true) { unsafe { return NativeIsKeyPressed((int)key, repeat); } }
    /// a key with its modifiers, pressed where it is the caller's to take. global: wherever
    ///  the focus is, unless something that has the keyboard (a text field being typed in)
    ///  wants the keys itself. otherwise only while the window being drawn has the focus
    public static bool Shortcut(Key key, KeyMods mods = KeyMods.None, bool global = false) { unsafe { return NativeShortcut((int)key, (int)mods, global); } }
    public static KeyMods KeyMods { get { unsafe { return (KeyMods)NativeGetKeyMods(); } } }

    /// the last item's rectangle on the screen
    public static Vec2 ItemRectMin
    {
      get
      {
        float x, y;
        unsafe { NativeGetItemRectMin(&x, &y); }
        return new Vec2(x, y);
      }
    }

    public static Vec2 ItemRectMax
    {
      get
      {
        float x, y;
        unsafe { NativeGetItemRectMax(&x, &y); }
        return new Vec2(x, y);
      }
    }

    public static Vec2 ItemRectSize
    {
      get
      {
        float x, y;
        unsafe { NativeGetItemRectSize(&x, &y); }
        return new Vec2(x, y);
      }
    }

    /// a min and max that are both 0 leave the value unbounded
    public static bool DragInt(string label, ref int value, float speed = 1f, int min = 0, int max = 0)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        fixed (int* value_ptr = &value)
        {
          return NativeDragInt(label_utf8.Pointer, value_ptr, speed, min, max);
        }
      }
    }

    public static bool DragFloat2(string label, ref Vec2 value, float speed = 1f, float min = 0f, float max = 0f)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        float* values = stackalloc float[2] { value.X, value.Y };
        bool changed = NativeDragFloat2(label_utf8.Pointer, values, speed, min, max);
        value = new Vec2(values[0], values[1]);
        return changed;
      }
    }

    public static bool DragFloat4(string label, ref Vec4 value, float speed = 1f, float min = 0f, float max = 0f)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        float* values = stackalloc float[4] { value.X, value.Y, value.Z, value.W };
        bool changed = NativeDragFloat4(label_utf8.Pointer, values, speed, min, max);
        value = new Vec4(values[0], values[1], values[2], values[3]);
        return changed;
      }
    }

    public static bool SliderFloat2(string label, ref Vec2 value, float min, float max)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        float* values = stackalloc float[2] { value.X, value.Y };
        bool changed = NativeSliderFloat2(label_utf8.Pointer, values, min, max);
        value = new Vec2(values[0], values[1]);
        return changed;
      }
    }

    public static bool SliderFloat3(string label, ref Vec3 value, float min, float max)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        float* values = stackalloc float[3] { value.X, value.Y, value.Z };
        bool changed = NativeSliderFloat3(label_utf8.Pointer, values, min, max);
        value = new Vec3(values[0], values[1], values[2]);
        return changed;
      }
    }

    public static bool SliderFloat4(string label, ref Vec4 value, float min, float max)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        float* values = stackalloc float[4] { value.X, value.Y, value.Z, value.W };
        bool changed = NativeSliderFloat4(label_utf8.Pointer, values, min, max);
        value = new Vec4(values[0], values[1], values[2], values[3]);
        return changed;
      }
    }

    /// between BeginDragDropTarget and EndDragDropTarget: the bytes that were dropped here,
    ///  when what was dropped is of that type. null while nothing of the type has been
    public static byte[]? AcceptDragDropPayload(string type, int flags = 0)
    {
      unsafe
      {
        using var type_utf8 = new Utf8Arg(type, stackalloc byte[Utf8Arg.StackSize]);
        int size = 0;
        void* data = NativeAcceptDragDropPayload(type_utf8.Pointer, flags, &size);
        if (data == null)
        {
          return null;
        }
        return new ReadOnlySpan<byte>(data, size).ToArray();
      }
    }

    public static void HelpMarker(string description)
    {
      if (IsItemHovered())
      {
        BeginTooltip();
        Text(description);
        EndTooltip();
      }
    }
  }
}