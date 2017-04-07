
## Backing up files

Here's an example of how to back up files. Let's say you have two directories, /home/person/files1 and /home/person/files2 that you would like to back up.

* Start GlacialBackup

* Choose **Create backup group...**

* Enter a group name, like my_files, and press Enter

* Enter the first directory and press Enter

* ![Screenshot create group](https://raw.githubusercontent.com/downpoured/projects_glacial_backup/master/doc/img/backup1.png)

* Now, let's edit the group options. Choose **Edit backup group...**

* Choose **Add/remove directories...**

* Choose the name of the group, **my_files**

* Choose **Add directory...**

* Enter the second directory (/home/person/files2) and press Enter

* ![Screenshot add dir](https://raw.githubusercontent.com/downpoured/projects_glacial_backup/master/doc/img/backup2.png)

* Choose **Back**

* Choose **Back** to return to the main menu.

We're now ready to start backing up files.

* Choose **Run backup...**

* Choose the name of the group, **my_files**

* Start backup

* ![Screenshot done backup](https://raw.githubusercontent.com/downpoured/projects_glacial_backup/master/doc/img/backup3.png)

When the backup is complete, it creates tar files in the "readytoupload" directory. Back up your data by saving the contents of this directory to cloud storage or an external drive. 

* ![Screenshot readytoupload](https://raw.githubusercontent.com/downpoured/projects_glacial_backup/master/doc/img/backupready.png)

* For example, use a command like this to sync to your Amazon S3 storage (after setting credentials)

        aws s3 sync /home/person/.local/share/glacial_backup/userdata/my_files/readytoupload s3://bucket/dir

* After the upload is complete, you can remove these local files if desired.

