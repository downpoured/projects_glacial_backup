
[Setup](setup.md) | [Backup](backup.md) | Restore | [Other features](other.md)

## Restoring files

[Restoring all files](restore_all.md)

[Restoring a specific file or directory](restore_partial.md)

[Restoring from a previous version](restore_prev.md)

## Restoring without GlacialBackup software 

* GlacialBackup was designed so that it should not be difficult to restore data -- even if GlacialBackup is not installed.

* For example, let's look inside one of the .tar files created by GlacialBackup.

    ![Screenshot inside tar](https://raw.githubusercontent.com/downpoured/projects_glacial_backup/master/doc/img/contents.png)

* As you can see, there is a "filenames.txt" that describes the contents of this .tar.

* If you wanted to restore song1.m4a, notice that it has number 00000002. So extract the file 00000002.file, rename it to song1.m4a, and this file is restored.

* If you wanted to restore text5.txt, notice that it has number 00000003. So look for the file 00000003.xz. Uncompress to get 00000003 and rename to text5.txt, and this file is restored.

* GlacialBackup archives should thus last many years into the future, because all files can be restored as long as software can read the standard tar, xz, and text formats.


