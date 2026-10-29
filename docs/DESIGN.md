# Design and mockup mapping

Source of truth for screens: the FocusType mockup (a Claude Design canvas, "FocusType — RLCD
Screens", `https://claude.ai/artifact/T8zGCdmbkZBKXvRYoEt3x9`, private to the owner's account).
14 artboards of 400x300, IBM Plex Sans/Mono, black ink on a light-grey paper. If you cannot open
it, the geometry and behaviour are recorded here and in `components/ui/ui.c`.

## Look

- Header 26 px: left title (13 px bold), right status or date (13 px), 2 px rule underneath.
- Footer 22 px: 1 px rule on top, keycap + label pairs (12 px), 14 px between pairs.
- Selected item: filled black with paper-coloured text and a `▸` marker.
- Editor: Plex Mono 17 px on 22 px lines, 12 px left margin, 2 px wide beam cursor, scrollbar with a
  1 px track and 3 px thumb at the right edge.
- Items are bordered boxes (2 px) with 7/10 px padding; Home card 57 px tall, other rows 39 px,
  5 px gaps. List rows 45 px (5 px padding, 19 px title line, 15 px subline).
- Only ink and paper: no greys, no animation. Meaning must not depend on colour.

## Screens

| ID | Screen | Status | Where |
|---|---|---|---|
| A1 | Boot / recovery draft found | not built | needs draft files and a boot check |
| A2 | Home | built | `ui_home`, `key_home` |
| B1 | Ctrl+N: empty note ("New note" in header, hint text) | built to the mockup, host render only: "Start typing…" after the cursor, dashed hint box, footer "Ctrl+K Look up / Esc Discard empty note". Not italic: the font set has no italic. Shown only while the note is new and empty | `ui_editor` (`empty_new`) |
| B2 | First line becomes the title | built | `ed_title`, header |
| B3 | Editor with save states | built | `ui_editor`, `key_editor` |
| B4 | Save failed banner | built (text generic, mockup says microSD) | `ui_editor`, `save_now` |
| C1 | Search from Home | built to the mockup (Find label, 5 rows with a snippet and the match bold and underlined); rendered on the host only, not yet seen on the board. Full text, ASCII case-insensitive. Status before the date ("Draft ·") is missing: needs metadata | `ui_search`, `key_search`, `notes_search` |
| C2 | All writing | built (no Details yet) | `ui_list`, `key_list` |
| D1 | Ctrl+K inside a note | built to the mockup (panel over the dimmed draft); rendered on the host only, not yet seen on the panel | `ui_dim`, `ui_lookup`, `key_lookup` |
| D2 | Read-only preview, Ctrl+Enter inserts a reference | built to the mockup; rendered on the host only. The mockup does not say what a reference looks like: it inserts `[[Title]]` at the cursor (my choice). The created date in the info line is missing (no metadata): it shows `edited <date> · N words` | `ui_preview`, `key_preview`, `insert_reference` |
| E1 | Details & actions | built to the mockup, rendered on the host only, not yet seen on the panel. Open with Right on a row of All writing (or on a Search result). Sub-screens the mockup does not draw are my own design, built from the Rename panel style: Change status (pick one of Draft, Developing, Revising, Done), Tags & category (two fields), Archive/Unarchive confirmation, Export message | `ui_details`, `ui_panel_*`, `key_detail` |
| E2 | Rename (title is metadata only, text never changes) | built to the mockup (panel over the dimmed Details), host render only | `ui_panel_form`, `key_detail` |
| F1 | Device status (battery shows "Not calibrated" until verified) | not built | needs ADC, RTC |
| F2 | Keyboard not detected | built | `ui_kbd_error` |

## Behaviour rules from the mockup notes

- Power on goes to Home; recovery appears only if a draft was interrupted. Home items: Continue
  writing, New note (Ctrl+N), All writing (Ctrl+O), Search (Ctrl+K).
- Ctrl+N then type immediately; the first line becomes the title; autosave and Ctrl+S. "Saved" must
  only appear after a real successful write. Esc on an empty note discards it.
- From Home, Enter on a search result opens the note straight into the editor (no draft to protect).
- In the editor: Ctrl+K opens look-up, Enter shows a read-only preview, Ctrl+Enter inserts a
  reference, Esc goes back to results, Esc again returns to the draft at the same cursor.
- A manual title (Rename) is metadata only; an empty manual title falls back to the first line.
- Battery reads "Not calibrated" until readings are verified.
- With no keyboard, actions use the physical KEY and PWR buttons ("Scan for keyboard" / "Hold PWR to
  power off").

## Key bindings implemented

| Context | Keys |
|---|---|
| Home | Up/Down select, Enter open, Ctrl+N new, Ctrl+O all writing, Ctrl+K search, Ctrl+T set clock |
| Search | typing and Backspace edit the query (results update 250 ms after the last key), Up/Down select, Enter open, Esc home |
| Set clock | Left/Right (or Tab) field, Up/Down change, PgUp/PgDn change by 10, Enter set, Esc cancel |
| All writing | Up/Down, PgUp/PgDn, Enter open, Esc home |
| Look up (Ctrl+K in the editor) | typing and Backspace edit the query, Up/Down select, Enter preview, Esc back to the draft (saved first) |
| Preview | Up/Down scroll one line, PgUp/PgDn one page, Home/End, Ctrl+Enter insert `[[Title]]` and return to the draft, Esc back to the results |
| Details | Up/Down select, Enter do, Esc back to where it was opened from. Panels: type and Backspace in text fields, Up/Down/Tab switch field, Enter save, Esc cancel |
| Editor | Ctrl+K look up, typing, Enter, Backspace, Delete, arrows, Home/End, PgUp/PgDn, Ctrl+S save, Esc save-and-leave |
| Save failed | Ctrl+S retry, Esc keep writing |

## Metadata storage (implemented)

Notes stay plain `.txt`. Status, category, tags, flag, manual title, archived and created time live in a
sidecar `ft-NNNN.meta` (`key=value` lines, unknown keys ignored), written atomically with `fs_save_atomic`, so the
text file is never touched by metadata edits. See `components/notes/include/note_meta.h`; host test `test_meta`.

Decisions that are mine, not the mockup's:
- Statuses: Draft (the default), Developing, Revising, Done.
- A tag is stored as `#word`; typing `foo, bar` gives `#foo #bar`.
- Archive hides a note from All writing and the Home shortcut. Search and Look up still find it, and Right on a
  Search result opens its Details, where the action reads "Unarchive". Nothing is deleted.
- Created time is written at the first save, only if the clock has been set (2025 or later). Older notes show a dash.
- Export to card only explains: the notes already live on the card, or no card is in use. Nothing is copied.
- All writing and Search rows show "Status · #tags · words" and "Status · date"; a flagged note shows a drawn flag
  shape (the font has no flag glyph).
