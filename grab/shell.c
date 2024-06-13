#include <types.h>
#include <kbd.h>
#include <printf.h>
#include <fs.h>
#include <fs-api.h>
#include <util.h>
#include <vga.h>
#include <ide.h>
#include <assert.h>
#include <panic.h>

struct {
	int rootdrive;
	int rootpartition;
} grab = { -1, -1 };

#define COLOR (BGND_BLACK | FGND_WHITE)

char *nextword(char *p, char **word);

static void ide_list()
{
    for (int x = 0; x < 4; x++) {
        if (ide_sel(x) < 0)
            continue;

        struct partition *partitions;
        if (!(partitions = ide_get_partitions())) {
            printf("(hd%d,?) ", x);
            continue;
        }

        for (int y = 0; y < 4; y++)
            if (partitions[y].nsectors)
                printf("(hd%d,msdos%d) ", x, y);
    }
    printf("\n");
}

static int getXY(char *s, int *X, int *Y, char *caller)
{
    // format: (hdX,Y) - partition Y in drive X

    if (strlen(s) < 7 || 
        s[0] != '(' || 
        s[1] != 'h' ||
        s[2] != 'd' || 
        !isdigit(s[3]) || 
        s[4] != ',' ||
        !isdigit(s[5]) || 
        s[6] != ')') {
        printf("%s: inalid (hdX,Y)\n", caller);
        return -1;
    }

    int x = s[3] - '0';
    int y = s[5] - '0';

    if (x >= 4) {
        printf("%s: invalid drive number: %d\n", caller, x);
        return -1;
    }

    if (y >= 4) {
        printf("%s: invalid partition number: %d\n", caller, y);
        return -1;
    }

    if (ide_sel(x) < 0) {
        printf("%s: drive doesn't exist: %d\n", caller, x);
        return -1;
    }

    struct partition *partitions;
    if (!(partitions = ide_get_partitions())) {
        printf("%s: not msdos: %d\n", caller, y);
        return -1;
    }

    if (!partitions[y].nsectors) {
        printf("%s: zero partition: %d\n", caller, y);
        return -1;
    }

    if (fs_init(&partitions[y], ide_read_lba, ide_write_lba, (printfunc)printf) < 0) {
        printf("%s: no fs detected in partition: %d\n", caller, y);
        return -1;
    }

	*X = x;
	*Y = y;

    return 0;
}

static void ls(char *args)
{
    char *path;
    args = nextword(args, &path);
    if (!path) {
        ide_list();
        return;
    }
    
    // path format: (hdX,Y) - partition Y in drive X

    int X;
    int Y;

    if (getXY(path, &X, &Y, "ls") < 0)
        return;

    path += 7;

    uint32_t inum = fs_lookup(path);
    if (inum == NULLINUM) {
        printf("ls: %s: no such file or directory\n", path);
        return;
    }
    uint32_t off = 0;
    for (;;) {
        struct dirent d;
        int n = fs_read(inum, &d, sizeof d, off);
		assert(n >= 0);
        if (!n)
            break;
        if (d.inum)
            printf("%s\n", d.name);
        off += sizeof d;
    }
}

static void cat(char *args)
{
	if (grab.rootdrive == -1) {
		printf("cat: root hasn't been set\n");
		return;
	}
    char *path;
    args = nextword(args, &path);
    if (!path) {
        printf("cat: no input file\n");
        return;
    }
    char buf[64];
    uint32_t off = 0;
    uint32_t inum = fs_lookup(path);
    if (inum == NULLINUM) {
        printf("cat: %s: no such file\n", path);
        return;
    }
	struct dinode di;
	assert(fs_geti(inum, &di) >= 0);
	if (di.type != T_REG) {
		printf("cat: not a regular file:\n", path);
		return;
	}
    for (;;) {
        int n = fs_read(inum, buf, 64-1, off);
		assert(n >= 0);
        off += n;
        if (n == 0)
            return;
        buf[n] = 0;
        printf("%s", buf);
    }
}

static void boot()
{
	if (grab.rootdrive == -1) {
		printf("cat: root hasn't been set\n");
		return;
	}

    uint32_t inum = fs_lookup("/boot");
	assert(inum != NULLINUM);

    vga_reset();
    uint32_t off = 0;
    int cnt = 0;
    struct dirent d;
    int n;
    for (;;cnt++) {
        n = fs_read(inum, &d, sizeof d, off);
		assert(n >= 0);
        if (!n)
            break;
        if (d.inum)
            printf("%s\n", d.name);
        off += sizeof d;
    }

    int row = 0;
    vga_hide_cursor();
    vga_set_color(row, BGND_BLUE | FGND_WHITE);

    for (;;) {
        key_event_t e = kbd_poll_event();
        if (TO_STRUCT(e).data == KEY_U_ARROW && row != 0) {
            vga_set_color(row, BGND_BLACK | FGND_WHITE);
            vga_set_color(--row, BGND_BLUE | FGND_WHITE);
        } else if (TO_STRUCT(e).data == KEY_D_ARROW && row < cnt - 1) {
            vga_set_color(row, BGND_BLACK | FGND_WHITE);
            vga_set_color(++row, BGND_BLUE | FGND_WHITE);
        } else if (TO_STRUCT(e).data == '\n') {
            vga_set_color(row, BGND_BLACK | FGND_WHITE);
            vga_reset();
            vga_show_cursor();
            break;
        }
    }

    n = fs_read(inum, &d, sizeof d, row * sizeof d);
    assert(n == sizeof d);
    printf("booting %s...\n", d.name);
    printf("loading system at 0x100000\n");

    char *p = (char *)0x100000;
    char buf[BLOCKSIZE];
    off = 0;
    for (;;) {
        n = fs_read(d.inum, buf, BLOCKSIZE, off);
        assert(n >= 0);
        if (!n)
            break;
        memcpy(p, buf, n);
        off += n;
        p += BLOCKSIZE;
    }

    ((void (*)(void))0x100000)();
}

static void set(char *args)
{
	char *name;
	char *value;

	// Replace '=' with ' '
	for (int i = 0; args[i]; i++)
		if (args[i] == '=')
			args[i] = ' ';

    args = nextword(args, &name);
    if (!name) {
        printf("set: no name specified\n");
        return;
    }

	args = nextword(args, &value);
    if (!value) {
        printf("set: no value specified\n");
        return;
    }

    // Allowed name-value pairs: root=(hdX,Y)
    // ... add here
	int X;
	int Y;

	if (!strncmp("root", name, 4)) {

		if (getXY(value, &X, &Y, "set") < 0)
			return;

		// Search for the 'boot' directory
		uint32_t inum = fs_lookup("/boot");
		if (inum == NULLINUM) {
			printf("set: /boot not found\n");
			return;
		}

		// /boot must be a non-empty directory
		struct dinode di;
		assert(fs_geti(inum, &di) >= 0);
		if (di.type != T_DIR) {
			printf("set: /boot is not a directoy\n");
			return;
		}

		if (di.size == 0) {
			printf("set: /boot is empty\n");
			return;
		}

		grab.rootdrive = X;
		grab.rootpartition = Y;
	} else {
        printf("set: invalid name: %s\n", name);
    }
}

// Command buffer length
#define CMDLEN (80 * 2)

void process_command(char *cmd);

void shell()
{
	// testing...
	set("root=(hd0,0)");
	boot();

    printf("Press enter to enter GRAB\n");
    kbd_poll_event();
    vga_reset();

    printf("grab> ");
    // Below variables define the command being entered
    int start = vga_get_cursor();           // Start position in 80*25 screen
    int off = 0;                            // Cursor offset from the *command start*
    char buf[CMDLEN] = {0};                 // Command buffer
    int len = 0;                            // Command length
    
    for (;;) {
        key_event_t e = kbd_poll_event();
        uint8_t data = apply_shift(TO_STRUCT(e).data, e);

        if (data == '\n') {
            buf[len] = 0; // Null-terminate the command
            printf("\n");
            process_command(buf);
            printf("grab> ");
            start = vga_get_cursor();
            off = 0;
            len = 0;
        } else if (isprint(data) && len < CMDLEN-1) {
            // Exmaple:
            // To insert 'a' at '3' in 12|3456  => 12|3456
            // 1. Right shift 3456 by 1         => 12| 3456
            // 2. Place 'a' into the gap        => 12|a3456
            // 3. Move the cursor right by 1    => 12a|3456
            // The above steps must be done to both
            // the buffer and the VGA text buffer!
            for (int i = len; i > off; i--) {
                vga_putchar(start + i, buf[i - 1], COLOR);
                buf[i] = buf[i - 1];
            }
            buf[off] = data;
            vga_putchar(start + off, data, COLOR);
            vga_set_cursor(vga_scroll(vga_get_cursor() + 1));
            off++;
            len++;
        } else if (data == KEY_L_ARROW && off > 0) {
            --off;
            vga_set_cursor(vga_scroll(vga_get_cursor() - 1));
        } else if (data == KEY_R_ARROW && off < len) {
            ++off;
            vga_set_cursor(vga_scroll(vga_get_cursor() + 1));
        } else if (data == '\b' && off > 0) {
            // Exmaple:
            // To delete '3' in 12|3456     => 123|456
            // 1. Left shift 456 by 1       => 124|566
            // 2. Remove the trailing 6     => 124|56
            // 3. Move the cursor left by 1 => 12|456
            // The above steps must be done to both
            // the buffer and the VGA text buffer!
            for (int i = off - 1; i < len; i++) {
                vga_putchar(start + i, buf[i + 1], COLOR);
                buf[i] = buf[i + 1];
            }
            vga_putchar(start + len - 1, ' ', COLOR);
            vga_set_cursor(vga_scroll(vga_get_cursor() - 1));
            off--;
            len--;
        }
    }
}

// Each call to this function consumes one word from 'buf.'
// The word consumed is returned by 'word.' The word returned is already null-terminated. 
// Return a pointer to the next word or 0 if there're no more words.
char *nextword(char *p, char **word)
{
    for (; *p && isspace(*p); p++);
    *word = p;
    if (!*p)
        *word = 0;
    for (; *p && !isspace(*p); p++);
    if (!*p)
        return 0;
    *p++ = 0; // Null-terminate this word.
    return p;
}

void process_command(char *cmd)
{
    char *this;
    char *next = cmd;
    next = nextword(next, &this);
    if (!this)
        return;
    if (!strncmp("ls", this, 2))
        ls(next);
    else if (!strncmp("boot", this, 4))
        boot();
    else if (!strncmp("set", this, 3))
        set(next);
}
