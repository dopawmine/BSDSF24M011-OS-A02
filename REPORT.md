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



### Q1: Compare the implementation complexity of "down then across" (vertical) and "across" (horizontal) printing. Which requires more pre-calculation, and why?

The vertical ("down then across") logic is more complex and needs more pre-calculation. Before printing anything, it must compute:

- `col_width = maxlen + spacing`
- `cols = terminal_width / col_width`
- `rows = ceil(total_files / cols)`

The number of rows must be known first because it decides where each column starts in the array. Each item is then found with index math, `filenames[c * rows + r]`, and the code must also check for an incomplete last column (`idx >= count`) and avoid padding the last item in a row. The printing is done with nested loops that jump through the array by `rows` positions.

The horizontal ("across") logic is simpler. It only needs `col_width`, and then walks the array in its normal order (0, 1, 2, ...). A running `pos` counter tracks the horizontal cursor, and when `pos + col_width` would exceed the terminal width, it prints a newline and resets `pos` to 0. It does not need to compute the number of rows or columns up front, and it has no index math or incomplete-column case, because the filenames are already in the order they are printed.

Both need all filenames read into an array first, so the longest name is known, but only the vertical mode needs the row/column calculation.

### Q2: Describe the strategy used to manage the different display modes (`-l`, `-x`, default). How did the program decide which function to call?

I used an enum, `display_mode_t`, with three values: `MODE_DEFAULT`, `MODE_LONG` and `MODE_HORIZONTAL`. In `main()`, `getopt(argc, argv, "lx")` parses the options and sets a single `mode` variable: `-l` sets `MODE_LONG` and `-x` sets `MODE_HORIZONTAL`. If no option is given, the mode stays `MODE_DEFAULT`. If both options are given, the last one wins, as in the real `ls`.

`main()` passes `mode` to `do_ls(dir, mode)`. This function reads all the filenames into a dynamically allocated array and tracks the longest name. It then uses a `switch (mode)` to choose the printing function:

- `MODE_LONG` calls `do_ls_long()`
- `MODE_HORIZONTAL` calls `print_horizontal()`
- `MODE_DEFAULT` calls `print_vertical()`

A single enum variable with a `switch` keeps the decision in one place, avoids conflicting flags, and makes it easy to add new display modes later by adding one enum value and one `case`.








### Q1: Why must all directory entries be read into memory before sorting? What are the drawbacks for directories with millions of files?

`readdir()` returns entries in whatever order the filesystem stores them, not alphabetical order, and it hands them out one at a time. The program cannot know where an entry belongs in the sorted order until it has seen every other entry. For example, the very last entry read could be alphabetically first. Sorting therefore needs the complete list in memory, so all names are read into a dynamically allocated array and sorted with `qsort()` before anything is displayed.

For directories with millions of files this approach has drawbacks:

- **High memory usage:** every filename is copied with `strdup()`, and the pointer array also takes space. A million names of about 20 bytes each is tens of megabytes, plus the allocator's overhead per string.
- **Delayed output:** nothing is printed until the whole directory has been read and sorted, so the user waits with no feedback. An unsorted `ls` could start printing immediately.
- **Sorting cost:** `qsort()` takes O(n log n) comparisons, which becomes noticeable for millions of entries.
- **Allocation failure:** if memory runs out, `realloc()` or `strdup()` fails and the program cannot list the directory at all.

### Q2: Explain the purpose and signature of the comparison function required by `qsort()`. How does it work, and why must it take `const void *` arguments?

`qsort()` is a generic function that can sort an array of any type, but it does not know how to compare elements of that type. The comparison function tells it how to order two elements. Its signature is:

```c
int compare_names(const void *a, const void *b);
```

`qsort()` calls it with pointers to two **elements of the array**. Since each element of our array is a `char *` (a string pointer), each argument is really a pointer to a string pointer, i.e. a `char **`. The function casts and dereferences them to get the strings, then compares them with `strcmp()`:

```c
int compare_names(const void *a, const void *b)
{
    const char *s1 = *(const char **)a;
    const char *s2 = *(const char **)b;
    return strcmp(s1, s2);
}
```

The return value tells `qsort()` the order: negative if the first element should come before the second, zero if they are equal, and positive if the first should come after the second.

The arguments are `const void *` because `qsort()` is type-independent. A `void *` can point to any type, so the same `qsort()` works for ints, structs or strings, and the comparison function casts the pointers back to the real type. The `const` promises that the comparison will not modify the elements being compared. `strcmp` cannot be passed directly, because it expects `const char *` arguments, while `qsort()` supplies `char **` pointers, so it would compare the pointer bytes instead of the strings.
