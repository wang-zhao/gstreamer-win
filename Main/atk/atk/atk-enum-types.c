


#include <atk.h>

/* enumerations from "/E/Workspace/EaseTele/EaseTeleAVSolution/GStreamer/Main/atk/atk/atkhyperlink.h" */
GType
atk_hyperlink_state_flags_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GFlagsValue values[] = {
      { ATK_HYPERLINK_IS_INLINE, "ATK_HYPERLINK_IS_INLINE", "inline" },
      { 0, NULL, NULL }
    };
    etype = g_flags_register_static ("AtkHyperlinkStateFlags", values);
  }
  return etype;
}

/* enumerations from "/E/Workspace/EaseTele/EaseTeleAVSolution/GStreamer/Main/atk/atk/atkobject.h" */
GType
atk_role_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { ATK_ROLE_INVALID, "ATK_ROLE_INVALID", "invalid" },
      { ATK_ROLE_ACCEL_LABEL, "ATK_ROLE_ACCEL_LABEL", "accel-label" },
      { ATK_ROLE_ALERT, "ATK_ROLE_ALERT", "alert" },
      { ATK_ROLE_ANIMATION, "ATK_ROLE_ANIMATION", "animation" },
      { ATK_ROLE_ARROW, "ATK_ROLE_ARROW", "arrow" },
      { ATK_ROLE_CALENDAR, "ATK_ROLE_CALENDAR", "calendar" },
      { ATK_ROLE_CANVAS, "ATK_ROLE_CANVAS", "canvas" },
      { ATK_ROLE_CHECK_BOX, "ATK_ROLE_CHECK_BOX", "check-box" },
      { ATK_ROLE_CHECK_MENU_ITEM, "ATK_ROLE_CHECK_MENU_ITEM", "check-menu-item" },
      { ATK_ROLE_COLOR_CHOOSER, "ATK_ROLE_COLOR_CHOOSER", "color-chooser" },
      { ATK_ROLE_COLUMN_HEADER, "ATK_ROLE_COLUMN_HEADER", "column-header" },
      { ATK_ROLE_COMBO_BOX, "ATK_ROLE_COMBO_BOX", "combo-box" },
      { ATK_ROLE_DATE_EDITOR, "ATK_ROLE_DATE_EDITOR", "date-editor" },
      { ATK_ROLE_DESKTOP_ICON, "ATK_ROLE_DESKTOP_ICON", "desktop-icon" },
      { ATK_ROLE_DESKTOP_FRAME, "ATK_ROLE_DESKTOP_FRAME", "desktop-frame" },
      { ATK_ROLE_DIAL, "ATK_ROLE_DIAL", "dial" },
      { ATK_ROLE_DIALOG, "ATK_ROLE_DIALOG", "dialog" },
      { ATK_ROLE_DIRECTORY_PANE, "ATK_ROLE_DIRECTORY_PANE", "directory-pane" },
      { ATK_ROLE_DRAWING_AREA, "ATK_ROLE_DRAWING_AREA", "drawing-area" },
      { ATK_ROLE_FILE_CHOOSER, "ATK_ROLE_FILE_CHOOSER", "file-chooser" },
      { ATK_ROLE_FILLER, "ATK_ROLE_FILLER", "filler" },
      { ATK_ROLE_FONT_CHOOSER, "ATK_ROLE_FONT_CHOOSER", "font-chooser" },
      { ATK_ROLE_FRAME, "ATK_ROLE_FRAME", "frame" },
      { ATK_ROLE_GLASS_PANE, "ATK_ROLE_GLASS_PANE", "glass-pane" },
      { ATK_ROLE_HTML_CONTAINER, "ATK_ROLE_HTML_CONTAINER", "html-container" },
      { ATK_ROLE_ICON, "ATK_ROLE_ICON", "icon" },
      { ATK_ROLE_IMAGE, "ATK_ROLE_IMAGE", "image" },
      { ATK_ROLE_INTERNAL_FRAME, "ATK_ROLE_INTERNAL_FRAME", "internal-frame" },
      { ATK_ROLE_LABEL, "ATK_ROLE_LABEL", "label" },
      { ATK_ROLE_LAYERED_PANE, "ATK_ROLE_LAYERED_PANE", "layered-pane" },
      { ATK_ROLE_LIST, "ATK_ROLE_LIST", "list" },
      { ATK_ROLE_LIST_ITEM, "ATK_ROLE_LIST_ITEM", "list-item" },
      { ATK_ROLE_MENU, "ATK_ROLE_MENU", "menu" },
      { ATK_ROLE_MENU_BAR, "ATK_ROLE_MENU_BAR", "menu-bar" },
      { ATK_ROLE_MENU_ITEM, "ATK_ROLE_MENU_ITEM", "menu-item" },
      { ATK_ROLE_OPTION_PANE, "ATK_ROLE_OPTION_PANE", "option-pane" },
      { ATK_ROLE_PAGE_TAB, "ATK_ROLE_PAGE_TAB", "page-tab" },
      { ATK_ROLE_PAGE_TAB_LIST, "ATK_ROLE_PAGE_TAB_LIST", "page-tab-list" },
      { ATK_ROLE_PANEL, "ATK_ROLE_PANEL", "panel" },
      { ATK_ROLE_PASSWORD_TEXT, "ATK_ROLE_PASSWORD_TEXT", "password-text" },
      { ATK_ROLE_POPUP_MENU, "ATK_ROLE_POPUP_MENU", "popup-menu" },
      { ATK_ROLE_PROGRESS_BAR, "ATK_ROLE_PROGRESS_BAR", "progress-bar" },
      { ATK_ROLE_PUSH_BUTTON, "ATK_ROLE_PUSH_BUTTON", "push-button" },
      { ATK_ROLE_RADIO_BUTTON, "ATK_ROLE_RADIO_BUTTON", "radio-button" },
      { ATK_ROLE_RADIO_MENU_ITEM, "ATK_ROLE_RADIO_MENU_ITEM", "radio-menu-item" },
      { ATK_ROLE_ROOT_PANE, "ATK_ROLE_ROOT_PANE", "root-pane" },
      { ATK_ROLE_ROW_HEADER, "ATK_ROLE_ROW_HEADER", "row-header" },
      { ATK_ROLE_SCROLL_BAR, "ATK_ROLE_SCROLL_BAR", "scroll-bar" },
      { ATK_ROLE_SCROLL_PANE, "ATK_ROLE_SCROLL_PANE", "scroll-pane" },
      { ATK_ROLE_SEPARATOR, "ATK_ROLE_SEPARATOR", "separator" },
      { ATK_ROLE_SLIDER, "ATK_ROLE_SLIDER", "slider" },
      { ATK_ROLE_SPLIT_PANE, "ATK_ROLE_SPLIT_PANE", "split-pane" },
      { ATK_ROLE_SPIN_BUTTON, "ATK_ROLE_SPIN_BUTTON", "spin-button" },
      { ATK_ROLE_STATUSBAR, "ATK_ROLE_STATUSBAR", "statusbar" },
      { ATK_ROLE_TABLE, "ATK_ROLE_TABLE", "table" },
      { ATK_ROLE_TABLE_CELL, "ATK_ROLE_TABLE_CELL", "table-cell" },
      { ATK_ROLE_TABLE_COLUMN_HEADER, "ATK_ROLE_TABLE_COLUMN_HEADER", "table-column-header" },
      { ATK_ROLE_TABLE_ROW_HEADER, "ATK_ROLE_TABLE_ROW_HEADER", "table-row-header" },
      { ATK_ROLE_TEAR_OFF_MENU_ITEM, "ATK_ROLE_TEAR_OFF_MENU_ITEM", "tear-off-menu-item" },
      { ATK_ROLE_TERMINAL, "ATK_ROLE_TERMINAL", "terminal" },
      { ATK_ROLE_TEXT, "ATK_ROLE_TEXT", "text" },
      { ATK_ROLE_TOGGLE_BUTTON, "ATK_ROLE_TOGGLE_BUTTON", "toggle-button" },
      { ATK_ROLE_TOOL_BAR, "ATK_ROLE_TOOL_BAR", "tool-bar" },
      { ATK_ROLE_TOOL_TIP, "ATK_ROLE_TOOL_TIP", "tool-tip" },
      { ATK_ROLE_TREE, "ATK_ROLE_TREE", "tree" },
      { ATK_ROLE_TREE_TABLE, "ATK_ROLE_TREE_TABLE", "tree-table" },
      { ATK_ROLE_UNKNOWN, "ATK_ROLE_UNKNOWN", "unknown" },
      { ATK_ROLE_VIEWPORT, "ATK_ROLE_VIEWPORT", "viewport" },
      { ATK_ROLE_WINDOW, "ATK_ROLE_WINDOW", "window" },
      { ATK_ROLE_HEADER, "ATK_ROLE_HEADER", "header" },
      { ATK_ROLE_FOOTER, "ATK_ROLE_FOOTER", "footer" },
      { ATK_ROLE_PARAGRAPH, "ATK_ROLE_PARAGRAPH", "paragraph" },
      { ATK_ROLE_RULER, "ATK_ROLE_RULER", "ruler" },
      { ATK_ROLE_APPLICATION, "ATK_ROLE_APPLICATION", "application" },
      { ATK_ROLE_AUTOCOMPLETE, "ATK_ROLE_AUTOCOMPLETE", "autocomplete" },
      { ATK_ROLE_EDITBAR, "ATK_ROLE_EDITBAR", "editbar" },
      { ATK_ROLE_EMBEDDED, "ATK_ROLE_EMBEDDED", "embedded" },
      { ATK_ROLE_ENTRY, "ATK_ROLE_ENTRY", "entry" },
      { ATK_ROLE_CHART, "ATK_ROLE_CHART", "chart" },
      { ATK_ROLE_CAPTION, "ATK_ROLE_CAPTION", "caption" },
      { ATK_ROLE_DOCUMENT_FRAME, "ATK_ROLE_DOCUMENT_FRAME", "document-frame" },
      { ATK_ROLE_HEADING, "ATK_ROLE_HEADING", "heading" },
      { ATK_ROLE_PAGE, "ATK_ROLE_PAGE", "page" },
      { ATK_ROLE_SECTION, "ATK_ROLE_SECTION", "section" },
      { ATK_ROLE_REDUNDANT_OBJECT, "ATK_ROLE_REDUNDANT_OBJECT", "redundant-object" },
      { ATK_ROLE_FORM, "ATK_ROLE_FORM", "form" },
      { ATK_ROLE_LINK, "ATK_ROLE_LINK", "link" },
      { ATK_ROLE_INPUT_METHOD_WINDOW, "ATK_ROLE_INPUT_METHOD_WINDOW", "input-method-window" },
      { ATK_ROLE_TABLE_ROW, "ATK_ROLE_TABLE_ROW", "table-row" },
      { ATK_ROLE_TREE_ITEM, "ATK_ROLE_TREE_ITEM", "tree-item" },
      { ATK_ROLE_DOCUMENT_SPREADSHEET, "ATK_ROLE_DOCUMENT_SPREADSHEET", "document-spreadsheet" },
      { ATK_ROLE_DOCUMENT_PRESENTATION, "ATK_ROLE_DOCUMENT_PRESENTATION", "document-presentation" },
      { ATK_ROLE_DOCUMENT_TEXT, "ATK_ROLE_DOCUMENT_TEXT", "document-text" },
      { ATK_ROLE_DOCUMENT_WEB, "ATK_ROLE_DOCUMENT_WEB", "document-web" },
      { ATK_ROLE_DOCUMENT_EMAIL, "ATK_ROLE_DOCUMENT_EMAIL", "document-email" },
      { ATK_ROLE_COMMENT, "ATK_ROLE_COMMENT", "comment" },
      { ATK_ROLE_LIST_BOX, "ATK_ROLE_LIST_BOX", "list-box" },
      { ATK_ROLE_GROUPING, "ATK_ROLE_GROUPING", "grouping" },
      { ATK_ROLE_IMAGE_MAP, "ATK_ROLE_IMAGE_MAP", "image-map" },
      { ATK_ROLE_NOTIFICATION, "ATK_ROLE_NOTIFICATION", "notification" },
      { ATK_ROLE_INFO_BAR, "ATK_ROLE_INFO_BAR", "info-bar" },
      { ATK_ROLE_LAST_DEFINED, "ATK_ROLE_LAST_DEFINED", "last-defined" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static ("AtkRole", values);
  }
  return etype;
}
GType
atk_layer_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { ATK_LAYER_INVALID, "ATK_LAYER_INVALID", "invalid" },
      { ATK_LAYER_BACKGROUND, "ATK_LAYER_BACKGROUND", "background" },
      { ATK_LAYER_CANVAS, "ATK_LAYER_CANVAS", "canvas" },
      { ATK_LAYER_WIDGET, "ATK_LAYER_WIDGET", "widget" },
      { ATK_LAYER_MDI, "ATK_LAYER_MDI", "mdi" },
      { ATK_LAYER_POPUP, "ATK_LAYER_POPUP", "popup" },
      { ATK_LAYER_OVERLAY, "ATK_LAYER_OVERLAY", "overlay" },
      { ATK_LAYER_WINDOW, "ATK_LAYER_WINDOW", "window" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static ("AtkLayer", values);
  }
  return etype;
}

/* enumerations from "/E/Workspace/EaseTele/EaseTeleAVSolution/GStreamer/Main/atk/atk/atkrelationtype.h" */
GType
atk_relation_type_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { ATK_RELATION_NULL, "ATK_RELATION_NULL", "null" },
      { ATK_RELATION_CONTROLLED_BY, "ATK_RELATION_CONTROLLED_BY", "controlled-by" },
      { ATK_RELATION_CONTROLLER_FOR, "ATK_RELATION_CONTROLLER_FOR", "controller-for" },
      { ATK_RELATION_LABEL_FOR, "ATK_RELATION_LABEL_FOR", "label-for" },
      { ATK_RELATION_LABELLED_BY, "ATK_RELATION_LABELLED_BY", "labelled-by" },
      { ATK_RELATION_MEMBER_OF, "ATK_RELATION_MEMBER_OF", "member-of" },
      { ATK_RELATION_NODE_CHILD_OF, "ATK_RELATION_NODE_CHILD_OF", "node-child-of" },
      { ATK_RELATION_FLOWS_TO, "ATK_RELATION_FLOWS_TO", "flows-to" },
      { ATK_RELATION_FLOWS_FROM, "ATK_RELATION_FLOWS_FROM", "flows-from" },
      { ATK_RELATION_SUBWINDOW_OF, "ATK_RELATION_SUBWINDOW_OF", "subwindow-of" },
      { ATK_RELATION_EMBEDS, "ATK_RELATION_EMBEDS", "embeds" },
      { ATK_RELATION_EMBEDDED_BY, "ATK_RELATION_EMBEDDED_BY", "embedded-by" },
      { ATK_RELATION_POPUP_FOR, "ATK_RELATION_POPUP_FOR", "popup-for" },
      { ATK_RELATION_PARENT_WINDOW_OF, "ATK_RELATION_PARENT_WINDOW_OF", "parent-window-of" },
      { ATK_RELATION_DESCRIBED_BY, "ATK_RELATION_DESCRIBED_BY", "described-by" },
      { ATK_RELATION_DESCRIPTION_FOR, "ATK_RELATION_DESCRIPTION_FOR", "description-for" },
      { ATK_RELATION_NODE_PARENT_OF, "ATK_RELATION_NODE_PARENT_OF", "node-parent-of" },
      { ATK_RELATION_LAST_DEFINED, "ATK_RELATION_LAST_DEFINED", "last-defined" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static ("AtkRelationType", values);
  }
  return etype;
}

/* enumerations from "/E/Workspace/EaseTele/EaseTeleAVSolution/GStreamer/Main/atk/atk/atkstate.h" */
GType
atk_state_type_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { ATK_STATE_INVALID, "ATK_STATE_INVALID", "invalid" },
      { ATK_STATE_ACTIVE, "ATK_STATE_ACTIVE", "active" },
      { ATK_STATE_ARMED, "ATK_STATE_ARMED", "armed" },
      { ATK_STATE_BUSY, "ATK_STATE_BUSY", "busy" },
      { ATK_STATE_CHECKED, "ATK_STATE_CHECKED", "checked" },
      { ATK_STATE_DEFUNCT, "ATK_STATE_DEFUNCT", "defunct" },
      { ATK_STATE_EDITABLE, "ATK_STATE_EDITABLE", "editable" },
      { ATK_STATE_ENABLED, "ATK_STATE_ENABLED", "enabled" },
      { ATK_STATE_EXPANDABLE, "ATK_STATE_EXPANDABLE", "expandable" },
      { ATK_STATE_EXPANDED, "ATK_STATE_EXPANDED", "expanded" },
      { ATK_STATE_FOCUSABLE, "ATK_STATE_FOCUSABLE", "focusable" },
      { ATK_STATE_FOCUSED, "ATK_STATE_FOCUSED", "focused" },
      { ATK_STATE_HORIZONTAL, "ATK_STATE_HORIZONTAL", "horizontal" },
      { ATK_STATE_ICONIFIED, "ATK_STATE_ICONIFIED", "iconified" },
      { ATK_STATE_MODAL, "ATK_STATE_MODAL", "modal" },
      { ATK_STATE_MULTI_LINE, "ATK_STATE_MULTI_LINE", "multi-line" },
      { ATK_STATE_MULTISELECTABLE, "ATK_STATE_MULTISELECTABLE", "multiselectable" },
      { ATK_STATE_OPAQUE, "ATK_STATE_OPAQUE", "opaque" },
      { ATK_STATE_PRESSED, "ATK_STATE_PRESSED", "pressed" },
      { ATK_STATE_RESIZABLE, "ATK_STATE_RESIZABLE", "resizable" },
      { ATK_STATE_SELECTABLE, "ATK_STATE_SELECTABLE", "selectable" },
      { ATK_STATE_SELECTED, "ATK_STATE_SELECTED", "selected" },
      { ATK_STATE_SENSITIVE, "ATK_STATE_SENSITIVE", "sensitive" },
      { ATK_STATE_SHOWING, "ATK_STATE_SHOWING", "showing" },
      { ATK_STATE_SINGLE_LINE, "ATK_STATE_SINGLE_LINE", "single-line" },
      { ATK_STATE_STALE, "ATK_STATE_STALE", "stale" },
      { ATK_STATE_TRANSIENT, "ATK_STATE_TRANSIENT", "transient" },
      { ATK_STATE_VERTICAL, "ATK_STATE_VERTICAL", "vertical" },
      { ATK_STATE_VISIBLE, "ATK_STATE_VISIBLE", "visible" },
      { ATK_STATE_MANAGES_DESCENDANTS, "ATK_STATE_MANAGES_DESCENDANTS", "manages-descendants" },
      { ATK_STATE_INDETERMINATE, "ATK_STATE_INDETERMINATE", "indeterminate" },
      { ATK_STATE_TRUNCATED, "ATK_STATE_TRUNCATED", "truncated" },
      { ATK_STATE_REQUIRED, "ATK_STATE_REQUIRED", "required" },
      { ATK_STATE_INVALID_ENTRY, "ATK_STATE_INVALID_ENTRY", "invalid-entry" },
      { ATK_STATE_SUPPORTS_AUTOCOMPLETION, "ATK_STATE_SUPPORTS_AUTOCOMPLETION", "supports-autocompletion" },
      { ATK_STATE_SELECTABLE_TEXT, "ATK_STATE_SELECTABLE_TEXT", "selectable-text" },
      { ATK_STATE_DEFAULT, "ATK_STATE_DEFAULT", "default" },
      { ATK_STATE_ANIMATED, "ATK_STATE_ANIMATED", "animated" },
      { ATK_STATE_VISITED, "ATK_STATE_VISITED", "visited" },
      { ATK_STATE_LAST_DEFINED, "ATK_STATE_LAST_DEFINED", "last-defined" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static ("AtkStateType", values);
  }
  return etype;
}

/* enumerations from "/E/Workspace/EaseTele/EaseTeleAVSolution/GStreamer/Main/atk/atk/atktext.h" */
GType
atk_text_attribute_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { ATK_TEXT_ATTR_INVALID, "ATK_TEXT_ATTR_INVALID", "invalid" },
      { ATK_TEXT_ATTR_LEFT_MARGIN, "ATK_TEXT_ATTR_LEFT_MARGIN", "left-margin" },
      { ATK_TEXT_ATTR_RIGHT_MARGIN, "ATK_TEXT_ATTR_RIGHT_MARGIN", "right-margin" },
      { ATK_TEXT_ATTR_INDENT, "ATK_TEXT_ATTR_INDENT", "indent" },
      { ATK_TEXT_ATTR_INVISIBLE, "ATK_TEXT_ATTR_INVISIBLE", "invisible" },
      { ATK_TEXT_ATTR_EDITABLE, "ATK_TEXT_ATTR_EDITABLE", "editable" },
      { ATK_TEXT_ATTR_PIXELS_ABOVE_LINES, "ATK_TEXT_ATTR_PIXELS_ABOVE_LINES", "pixels-above-lines" },
      { ATK_TEXT_ATTR_PIXELS_BELOW_LINES, "ATK_TEXT_ATTR_PIXELS_BELOW_LINES", "pixels-below-lines" },
      { ATK_TEXT_ATTR_PIXELS_INSIDE_WRAP, "ATK_TEXT_ATTR_PIXELS_INSIDE_WRAP", "pixels-inside-wrap" },
      { ATK_TEXT_ATTR_BG_FULL_HEIGHT, "ATK_TEXT_ATTR_BG_FULL_HEIGHT", "bg-full-height" },
      { ATK_TEXT_ATTR_RISE, "ATK_TEXT_ATTR_RISE", "rise" },
      { ATK_TEXT_ATTR_UNDERLINE, "ATK_TEXT_ATTR_UNDERLINE", "underline" },
      { ATK_TEXT_ATTR_STRIKETHROUGH, "ATK_TEXT_ATTR_STRIKETHROUGH", "strikethrough" },
      { ATK_TEXT_ATTR_SIZE, "ATK_TEXT_ATTR_SIZE", "size" },
      { ATK_TEXT_ATTR_SCALE, "ATK_TEXT_ATTR_SCALE", "scale" },
      { ATK_TEXT_ATTR_WEIGHT, "ATK_TEXT_ATTR_WEIGHT", "weight" },
      { ATK_TEXT_ATTR_LANGUAGE, "ATK_TEXT_ATTR_LANGUAGE", "language" },
      { ATK_TEXT_ATTR_FAMILY_NAME, "ATK_TEXT_ATTR_FAMILY_NAME", "family-name" },
      { ATK_TEXT_ATTR_BG_COLOR, "ATK_TEXT_ATTR_BG_COLOR", "bg-color" },
      { ATK_TEXT_ATTR_FG_COLOR, "ATK_TEXT_ATTR_FG_COLOR", "fg-color" },
      { ATK_TEXT_ATTR_BG_STIPPLE, "ATK_TEXT_ATTR_BG_STIPPLE", "bg-stipple" },
      { ATK_TEXT_ATTR_FG_STIPPLE, "ATK_TEXT_ATTR_FG_STIPPLE", "fg-stipple" },
      { ATK_TEXT_ATTR_WRAP_MODE, "ATK_TEXT_ATTR_WRAP_MODE", "wrap-mode" },
      { ATK_TEXT_ATTR_DIRECTION, "ATK_TEXT_ATTR_DIRECTION", "direction" },
      { ATK_TEXT_ATTR_JUSTIFICATION, "ATK_TEXT_ATTR_JUSTIFICATION", "justification" },
      { ATK_TEXT_ATTR_STRETCH, "ATK_TEXT_ATTR_STRETCH", "stretch" },
      { ATK_TEXT_ATTR_VARIANT, "ATK_TEXT_ATTR_VARIANT", "variant" },
      { ATK_TEXT_ATTR_STYLE, "ATK_TEXT_ATTR_STYLE", "style" },
      { ATK_TEXT_ATTR_LAST_DEFINED, "ATK_TEXT_ATTR_LAST_DEFINED", "last-defined" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static ("AtkTextAttribute", values);
  }
  return etype;
}
GType
atk_text_boundary_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { ATK_TEXT_BOUNDARY_CHAR, "ATK_TEXT_BOUNDARY_CHAR", "char" },
      { ATK_TEXT_BOUNDARY_WORD_START, "ATK_TEXT_BOUNDARY_WORD_START", "word-start" },
      { ATK_TEXT_BOUNDARY_WORD_END, "ATK_TEXT_BOUNDARY_WORD_END", "word-end" },
      { ATK_TEXT_BOUNDARY_SENTENCE_START, "ATK_TEXT_BOUNDARY_SENTENCE_START", "sentence-start" },
      { ATK_TEXT_BOUNDARY_SENTENCE_END, "ATK_TEXT_BOUNDARY_SENTENCE_END", "sentence-end" },
      { ATK_TEXT_BOUNDARY_LINE_START, "ATK_TEXT_BOUNDARY_LINE_START", "line-start" },
      { ATK_TEXT_BOUNDARY_LINE_END, "ATK_TEXT_BOUNDARY_LINE_END", "line-end" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static ("AtkTextBoundary", values);
  }
  return etype;
}
GType
atk_text_clip_type_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { ATK_TEXT_CLIP_NONE, "ATK_TEXT_CLIP_NONE", "none" },
      { ATK_TEXT_CLIP_MIN, "ATK_TEXT_CLIP_MIN", "min" },
      { ATK_TEXT_CLIP_MAX, "ATK_TEXT_CLIP_MAX", "max" },
      { ATK_TEXT_CLIP_BOTH, "ATK_TEXT_CLIP_BOTH", "both" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static ("AtkTextClipType", values);
  }
  return etype;
}

/* enumerations from "/E/Workspace/EaseTele/EaseTeleAVSolution/GStreamer/Main/atk/atk/atkutil.h" */
GType
atk_key_event_type_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { ATK_KEY_EVENT_PRESS, "ATK_KEY_EVENT_PRESS", "press" },
      { ATK_KEY_EVENT_RELEASE, "ATK_KEY_EVENT_RELEASE", "release" },
      { ATK_KEY_EVENT_LAST_DEFINED, "ATK_KEY_EVENT_LAST_DEFINED", "last-defined" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static ("AtkKeyEventType", values);
  }
  return etype;
}
GType
atk_coord_type_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { ATK_XY_SCREEN, "ATK_XY_SCREEN", "screen" },
      { ATK_XY_WINDOW, "ATK_XY_WINDOW", "window" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static ("AtkCoordType", values);
  }
  return etype;
}



