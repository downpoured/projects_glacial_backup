
[Setup](setup.md) | [Backup](backup.md) | [Restore](restore.md) | Compact | [Options](options.md) | [Other features](other.md)

### Compact archives

* You can compact archives to free up the hard drive space taken by GlacialBackup. Here is an example of when compaction is most useful:

* Bob has been using GlacialBackup for a few months and uploading the files to Amazon S3. He happens to go through his old files that he is backing up, and realizes that he is backing up his old screensaver programs that are taking up 500 megabytes! He doesn't need or want any of these files anymore. Bob can free up this 500Mb of space by running Compact.

* First, Bob deletes the old screensaver files.

* Then, Bob opens GlacialBackup and runs **Backup**.

* Bob continues using GlacialBackup every now and then.

* 30 days later, Bob opens GlacialBackup and chooses **More...** and **Compact backup data to save space** (the time is 30 days by default, to make sure the files truly aren't needed, can be changed in settings). 

* Bob chooses the backup group, and on the next screen chooses **Thorough Compaction**.

    ![Screenshot compact](https://raw.githubusercontent.com/downpoured/projects_glacial_backup/master/doc/img/comp1.png)

* GlacialBackup notifies Bob that 00002\_00004.tar and 00002\_00005.tar are no longer needed, and that 00002\_00006.tar has been compacted from 64Mb to 12Mb.

* Bob removes 00002\_00004.tar and 00002\_00005.tar from cloud storage, and uploads the new smaller 00002\_00006.tar in place of the existing version. If Bob is keeping all of his archives both locally and on Amazon S3, he can do this automatically with the command 

        aws s3 sync /home/(user)/.local/share/glacial_backup/userdata/(group)/readytoupload s3://bucket/dir --delete

* Now Bob has removed the unneeded data from his backup archives!
