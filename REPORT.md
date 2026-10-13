## Report Questions

### Q1: What is the difference between `stat()` and `lstat()`? When is `lstat()` more appropriate in `ls`?

Both system calls fill a `struct stat` with a file's metadata. The difference is how they treat symbolic links:

- `stat()` **follows** the symlink and returns information about the **target file**.
- `lstat()` does **not** follow it and returns information about the **link itself**.

`lstat()` is more appropriate in `ls -l`, because each directory entry must be described as it actually exists. A symlink should show type `l`, its own permissions and its own size, not those of its target. `stat()` would hide the fact that the entry is a link and would fail on a broken link. My implementation uses `lstat()` and prints `name -> target` using `readlink()`.

### Q2: How are bitwise operators and macros used to extract information from `st_mode`?

`st_mode` stores the file type in its upper bits and the special bits and permission bits (owner, group, others) in its lower 12 bits. Information is extracted with a bitwise AND (`&`) against a mask, which clears all bits except the ones being tested.

- **File type:** mask with `S_IFMT`, then compare with a type constant:
  `(st_mode & S_IFMT) == S_IFDIR` means it is a directory. Macros like `S_ISDIR()` and `S_ISREG()` do this internally.
- **Permissions:** each permission is a single bit, so test it directly:
  `st_mode & S_IRUSR` is non-zero if the owner has read permission. The same works for `S_IWUSR`, `S_IXUSR`, `S_IRGRP`, `S_IWGRP`, `S_IXGRP`, `S_IROTH`, `S_IWOTH`, `S_IXOTH`, and the special bits `S_ISUID`, `S_ISGID`, `S_ISVTX`.

Example: `st_mode = 0100644` gives `st_mode & S_IFMT == S_IFREG` (regular file), owner has `r` and `w` but not `x`, and group and others have only `r`. The result is `-rw-r--r--`.





### Q1: Explain the general logic for printing items in a "down then across" format. Why is a simple single loop through the filenames insufficient?

In "down then across" format, items fill the first column from top to bottom, then continue at the top of the next column. The program first calculates the layout:

- `col_width = maxlen + spacing`
- `cols = terminal_width / col_width`
- `rows = ceil(total_files / cols)`

It then prints **row by row**. For row `r` and column `c`, the item printed is `filenames[c * rows + r]`. So row 0 prints `filenames[0]`, `filenames[rows]`, `filenames[2*rows]`, and so on. Each item is padded to `col_width` so the next column lines up, and the loop stops when the index goes past the last file (which handles an incomplete last column).

A single loop over the array is insufficient because the terminal prints **left to right, one line at a time**, while the array is ordered **top to bottom, column by column**. Consecutive array elements belong in the same column (different rows), but a single loop would print them next to each other on the same row. That gives "across" ordering instead of "down then across". To get the correct layout, we need nested loops (rows outside, columns inside) that jump by `rows` positions in the array. This also requires knowing all filenames and the longest name in advance, which is why they are read into an array first.

### Q2: What is the purpose of `ioctl` here? What are the limitations of using only a fixed 80-column fallback?

`ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)` asks the kernel for the current size of the terminal and fills a `struct winsize`, where `ws.ws_col` gives the width in characters. The program uses this width to calculate how many columns fit on the screen.

If the program used only a fixed width of 80 columns:

- On a **wider terminal** (e.g., 200 columns), it would waste space and print fewer columns than could fit.
- On a **narrower terminal** (e.g., 60 columns), lines would exceed the screen width and wrap, breaking the column alignment.
- It would not adapt when the user **resizes** the terminal.

The fixed 80 is only suitable as a fallback, for example when output is piped or redirected to a file and `ioctl` fails because stdout is not a terminal.
