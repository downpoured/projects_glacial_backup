
[Setup](setup.md) | [Backup](backup.md) | Restore | [Compact](compact.md) | [Options](options.md) | [Other features](other.md)

### Restoring all files

* Start GlacialBackup

* Choose **Restore from another computer...**

* You will be shown a path that looks something like

        /home/person/.local/share/glacial_backup/userdata/example/readytoupload

* Create this directory. (If it already exists, delete everything under example/)

* Download all of the .tar and .db files and place them in this "readytoupload" directory.

* Press Enter and choose **Restore file(s)...**

* You should see "example" in the list. Choose **example**

* Type *, which means to restore all files, and press Enter

* Type the full path to an empty output directory, and press Enter

* Begin restore

  ![Screenshot begin restore](https://raw.githubusercontent.com/downpoured/projects_glacial_backup/master/doc/img/rest1.png)


### Restoring a specific file or directory

* If you only want to restore one file or directory, you don't need to download all of the .tar files

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

### Restoring a specific file or directory, from a previous version

* We'll again restore in two steps. The first step will not restore any files, but will show us which .tar files are necessary.

* Start GlacialBackup

* Choose **Restore from another computer...**

* You will be shown a path that looks something like

        /home/person/.local/share/glacial_backup/userdata/example/readytoupload

* Create this directory. (If it already exists, delete everything under example/)

* Download all .db files and place them in this "readytoupload" directory. There is one .db file for each past snapshot.

* Press Enter and choose **Restore from previous revisions...**

* You should see "example" in the list. Choose **example**

    ![Screenshot choose version](https://raw.githubusercontent.com/downpoured/projects_glacial_backup/master/doc/img/restchoose.png)

* Choose the desired version. (If compaction has been run, old deleted files might not be available.)

* We'll do this in two steps. The first step will not restore any files, but will show us which .tar files are necessary.

* If you want to restore one file, type the full path of where that file was located. If you want to restore one directory, type that directory with a trailing /\* , like /home/person/files2/\*

* When asked for output location, type the full path to an empty output directory, and press Enter

* Begin restore. We will see a warning that 0 files were restored, but that's OK.

    ![Screenshot need tar](https://raw.githubusercontent.com/downpoured/projects_glacial_backup/master/doc/img/rest3.png)

* See which .tar files are needed. In this example, we need 00001_00001.tar. Download the needed .tar files and save them to the 'readytoupload' directory that was created earlier.

* We can now run restore a second time. Choose **Restore from previous revisions...**

* Choose **example**

* Choose the same version that was chosen earlier.

* Type the same path or pattern that was typed earlier, like /home/person/files2/*

* Type the full path to an empty output directory, and press Enter

* Begin restore

* This time, as long as all .tar files are present, the files will be restored successfully.

    ![Screenshot path scope](https://raw.githubusercontent.com/downpoured/projects_glacial_backup/master/doc/img/rest4.png)

### Restoring without GlacialBackup software 

* GlacialBackup was designed so that it should not be difficult to restore data -- even if GlacialBackup is not installed.

* For example, let's look inside one of the .tar files created by GlacialBackup.

    ![Screenshot inside tar](https://raw.githubusercontent.com/downpoured/projects_glacial_backup/master/doc/img/contents.png)

* As you can see, there are .xz files, .file files, and a filenames.txt that describes the contents of this .tar.

* If you wanted to restore song1.m4a, notice that it has number 00000002. So extract the file 00000002.file, rename it to song1.m4a, and this file is restored.

* If you wanted to restore text5.txt, notice that it has number 00000003. So look for the file 00000003.xz. Uncompress to get 00000003 and rename to text5.txt, and this file is restored.

* GlacialBackup archives should thus last many years into the future, because all files can be restored as long as software can read the standard tar, xz, and text formats.
