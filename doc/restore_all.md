
[Setup](setup.md) | [Backup](backup.md) | [Restore](restore.md) | [Other features](other.md)

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
