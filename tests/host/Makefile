CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -O1
INC = -I../../components/st7305/include -I../../components/usb_kbd/include \
      -I../../components/editor/include -I../../components/util/include \
      -I../../components/fsutil/include -I../../components/notes/include \
      -I../../components/ui/include -I../../components/pcf85063/include -Istubs

TESTS = test_pack test_kbd_hid test_editor test_notes test_format test_pcf test_meta

all: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done

test_pack: test_pack.c ../../components/st7305/st7305_pack.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

test_kbd_hid: test_kbd_hid.c ../../components/usb_kbd/kbd_hid.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

test_editor: test_editor.c ../../components/editor/editor.c ../../components/util/utf8.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

test_notes: test_notes.c ../../components/notes/notes.c ../../components/fsutil/fsutil.c \
            ../../components/editor/editor.c ../../components/util/utf8.c ../../components/util/textfind.c ../../components/notes/note_meta.c
	$(CC) $(CFLAGS) -D_DEFAULT_SOURCE $(INC) -o $@ $^

test_format: test_format.c ../../components/ui/ui_format.c
	$(CC) $(CFLAGS) -D_DEFAULT_SOURCE $(INC) -o $@ $^

test_pcf: test_pcf.c ../../components/pcf85063/pcf_regs.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

test_meta: test_meta.c ../../components/notes/note_meta.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

clean:
	rm -f $(TESTS)

.PHONY: all clean
