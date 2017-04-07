
[Setup](setup.md) | [Backup](backup.md) | [Restore](restore.md) | [Other features](other.md)

### Restoring a specific file or directory

* Why different instructions for restoring some files versus restoring all files? If you only need to restore one file or directory, you don't need to download all of the .tar files.

* Start GlacialBackup

* Choose **Restore from another computer...**

* You will be shown a path that looks something like

        /home/person/.local/share/glacial_backup/userdata/example/readytoupload

* Create this directory. (If it already exists, delete everything under example/)

* Download the latest .db file (the .db file with the largest number) and place it in this "readytoupload" directory.

* Press Enter and choose **Restore file(s)...**

* You should see "example" in the list. Choose **example**

* We'll do this in two steps. The first step will not restore any files, but will show us which .tar files are necessary.

* If you want to restore one file, type the full path of where that file was located. If you want to restore one directory, type that directory with a trailing /* (see screenshot below for an example).

    ![Screenshot path scope](https://raw.githubusercontent.com/downpoured/projects_glacial_backup/master/doc/img/rest2.png)

* When asked for output location, type the full path to an empty output directory, and press Enter

* Begin restore. We will see a warning that 0 files were restored, but that's OK.

    ![Screenshot need tar](https://raw.githubusercontent.com/downpoured/projects_glacial_backup/master/doc/img/rest3.png)

* See which .tar files are needed. In this example, we need 00001_00001.tar. Download the needed .tar files and save them to the "readytoupload" directory that we created earlier.

* We'll now run restore a second time. Choose **Restore file(s)...**

* Choose **example**

* Type the same path or pattern that was typed earlier, like /home/person/files2/*

* Type the full path to an empty output directory, and press Enter

* Begin restore

* This time, as long as all .tar files are present, the files will be restored successfully.

    ![Screenshot path scope](https://raw.githubusercontent.com/downpoured/projects_glacial_backup/master/doc/img/rest4.png)