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
