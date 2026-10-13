#ifndef DISK_NAME_H
#define DISK_NAME_H

// Pure string helpers for multi-disk ADF sets (no libc, unit tested on the host).
//
// disk_name_step() turns ".../Game (Disk 1 of 3).adf" into ".../Game (Disk 2 of 3).adf".
// The disk index is the number (or single letter) after the word "disk" (any case,
// optionally separated by ' ', '_', '-' or '#'). Without one, the number right before
// the extension is used ("Game-1.adf", "Game_1.adf"). "of N" wraps N back to 1.

#ifdef __cplusplus
extern "C" {
#endif

#define DISK_NAME_DIGITS 1
#define DISK_NAME_LETTER 2

typedef struct disk_name_pos
{
	int start;   // offset of the index in the full path
	int len;     // characters in the index
	int kind;    // DISK_NAME_DIGITS or DISK_NAME_LETTER
	int value;   // numeric value (letters: A = 1)
	int total;   // N from "of N", 0 if unknown
} disk_name_pos;

static inline int disk_name_lower(char c)
{
	return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

static inline int disk_name_is_digit(char c)
{
	return c >= '0' && c <= '9';
}

static inline int disk_name_is_alpha(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline int disk_name_is_sep(char c)
{
	return c == ' ' || c == '_' || c == '-' || c == '#';
}

static inline int disk_name_strlen(const char *s)
{
	int n = 0;
	while (s[n]) n++;
	return n;
}

// Offset of the file name (after the last '/').
static inline int disk_name_base(const char *path)
{
	int base = 0;
	for (int i = 0; path[i]; i++)
		if (path[i] == '/') base = i + 1;
	return base;
}

// Offset of the extension dot in the file name, or the string length.
static inline int disk_name_ext(const char *path)
{
	int base = disk_name_base(path);
	int len = disk_name_strlen(path);
	for (int i = len - 1; i > base; i--)
		if (path[i] == '.') return i;
	return len;
}

// Reads "of N" after the index (separators allowed around "of").
static inline int disk_name_total(const char *path, int i, int end)
{
	while (i < end && disk_name_is_sep(path[i])) i++;
	if (i + 1 >= end || disk_name_lower(path[i]) != 'o' || disk_name_lower(path[i + 1]) != 'f') return 0;
	i += 2;
	while (i < end && disk_name_is_sep(path[i])) i++;
	int total = 0;
	int digits = 0;
	while (i < end && disk_name_is_digit(path[i]) && digits < 4) {
		total = total * 10 + (path[i] - '0');
		i++;
		digits++;
	}
	if (!digits || (i < end && disk_name_is_digit(path[i]))) return 0;
	return total;
}

// Finds the disk index in the file name. Returns 1 if found.
static inline int disk_name_find(const char *path, disk_name_pos *pos)
{
	int base = disk_name_base(path);
	int end = disk_name_ext(path);
	int found = 0;

	// The last "disk" in the name that is followed by a number or a lone letter
	for (int i = base; i + 4 <= end; i++) {
		if (disk_name_lower(path[i]) != 'd' || disk_name_lower(path[i + 1]) != 'i' ||
		    disk_name_lower(path[i + 2]) != 's' || disk_name_lower(path[i + 3]) != 'k')
			continue;
		int p = i + 4;
		while (p < end && disk_name_is_sep(path[p])) p++;
		if (p < end && disk_name_is_digit(path[p])) {
			int q = p;
			int value = 0;
			while (q < end && disk_name_is_digit(path[q])) {
				if (q - p < 6) value = value * 10 + (path[q] - '0');
				q++;
			}
			if (q - p > 4) continue; // not a disk number
			pos->start = p;
			pos->len = q - p;
			pos->kind = DISK_NAME_DIGITS;
			pos->value = value;
			pos->total = disk_name_total(path, q, end);
			found = 1;
		} else if (p < end && disk_name_is_alpha(path[p]) && (p + 1 >= end || !disk_name_is_alpha(path[p + 1]))) {
			pos->start = p;
			pos->len = 1;
			pos->kind = DISK_NAME_LETTER;
			pos->value = disk_name_lower(path[p]) - 'a' + 1;
			pos->total = 0;
			found = 1;
		}
	}
	if (found) return 1;

	// Fallback: the number right before the extension ("Game-1.adf")
	int q = end;
	int p = end;
	while (p > base && disk_name_is_digit(path[p - 1])) p--;
	if (p == q || q - p > 4) return 0;
	int value = 0;
	for (int i = p; i < q; i++) value = value * 10 + (path[i] - '0');
	pos->start = p;
	pos->len = q - p;
	pos->kind = DISK_NAME_DIGITS;
	pos->value = value;
	pos->total = 0;
	return 1;
}

// Writes the path of the next disk (or of disk 1 when to_first is set) into out.
// Returns 1 on success, 0 if the name has no disk index or out is too small.
static inline int disk_name_step(const char *path, char *out, int outsz, int to_first)
{
	disk_name_pos pos;
	if (!disk_name_find(path, &pos)) return 0;

	char num[8];
	int numlen = 0;
	if (pos.kind == DISK_NAME_LETTER) {
		char c = path[pos.start];
		int upper = (c >= 'A' && c <= 'Z');
		int v = to_first ? 1 : pos.value + 1;
		if (v > 26) v = 1;
		num[numlen++] = (char)((upper ? 'A' : 'a') + v - 1);
	} else {
		int v = to_first ? 1 : pos.value + 1;
		if (pos.total > 0 && v > pos.total) v = 1;
		// Keep zero padding ("Disk01" -> "Disk02")
		int width = (pos.len > 1 && path[pos.start] == '0') ? pos.len : 1;
		char rev[8];
		int r = 0;
		do {
			rev[r++] = (char)('0' + v % 10);
			v /= 10;
		} while (v && r < 6);
		while (r < width && r < 6) rev[r++] = '0';
		while (r) num[numlen++] = rev[--r];
	}

	int len = disk_name_strlen(path);
	int need = len - pos.len + numlen + 1;
	if (need > outsz) return 0;
	int o = 0;
	for (int i = 0; i < pos.start; i++) out[o++] = path[i];
	for (int i = 0; i < numlen; i++) out[o++] = num[i];
	for (int i = pos.start + pos.len; i < len; i++) out[o++] = path[i];
	out[o] = 0;
	return 1;
}

// Copies the file name without path and extension into out, at most width
// characters. Long names keep their start and end ("Monkey Isla~Disk 2 of 4").
static inline void disk_name_fit(const char *path, char *out, int width)
{
	int base = disk_name_base(path);
	int end = disk_name_ext(path);
	int len = end - base;
	int o = 0;
	if (width < 1) {
		out[0] = 0;
		return;
	}
	if (len <= width) {
		for (int i = base; i < end; i++) out[o++] = path[i];
	} else {
		int tail = width / 2;
		int head = width - tail - 1;
		for (int i = 0; i < head; i++) out[o++] = path[base + i];
		out[o++] = '~';
		for (int i = end - tail; i < end; i++) out[o++] = path[i];
	}
	out[o] = 0;
}

// Small string builders for OSD text (no sprintf in the BIOS)
static inline int disk_name_append(char *out, int o, int outsz, const char *s)
{
	while (*s && o < outsz - 1) out[o++] = *s++;
	out[o] = 0;
	return o;
}

static inline int disk_name_append_uint(char *out, int o, int outsz, unsigned int v, int min_digits)
{
	char rev[12];
	int r = 0;
	do {
		rev[r++] = (char)('0' + v % 10);
		v /= 10;
	} while (v && r < 10);
	while (r < min_digits && r < 10) rev[r++] = '0';
	while (r && o < outsz - 1) out[o++] = rev[--r];
	out[o] = 0;
	return o;
}

#ifdef __cplusplus
}
#endif

#endif // DISK_NAME_H
