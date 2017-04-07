
[Setup](setup.md) | [Backup](backup.md) | [Restore](restore.md) | [Compact](compact.md) | [Options](options.md) | Other features

## Other features

Chose **More...** to see these features.

#### View information

Shows a summary of backup information, for example:

![Screenshot view info](https://raw.githubusercontent.com/downpoured/projects_glacial_backup/master/doc/img/moreinfo.png)

#### View logs

Log text files are saved on disk, primarily for diagnostic purposes.

#### Verify archive integrity

Compares checksums to confirm the integrity of .tar files.

#### Run tests

Runs tests. Requires ffmpeg to be installed, see the prereqs in the last item on the [options](options.md) page.

#### Run backups with low-privilege account

We support running GlacialBackup in a secondary user account that has restricted privileges. This can be useful for running as a background task, or to minimize the impact of any security vulnerability in xz or ffmpeg (although, note that the use of ffmpeg is disabled by default).

#### Text file list of files to back up

Hidden feature: if the readytoupload directory for your group is at

        /home/person/.local/share/glacial_backup/userdata/my_files/readytoupload
        
Create a text file at

        /home/person/.local/share/glacial_backup/userdata/my_files/manual-file-list.txt
        
with a list of filepaths, one per line, and these files will be backed up.

