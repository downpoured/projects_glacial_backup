
[Setup](setup.md) | [Backup](backup.md) | [Restore](restore.md) | [Other features](other.md)

## Options

Choose **Edit backup group...** to set options.

#### Add/remove directories

Use this to set the base directories containing files to be backed up.

#### Add/remove exclusion patterns

You can add exclusion patterns to say that certain files or directories do not need to be backed up. For example, if the pattern \*.tmp is in this list, a file named test.tmp will be skipped. A pattern like /example/dir1/\* means that all files under this directory will be skipped. A pattern like \*abc\* means that any file with 'abc' in its path will be skipped.

A few exclusion patterns exist by default to skip temporary files like .pyc. 

#### Set how long to keep previous file versions

By default, GlacialBackup keeps previous versions of a file for 30 days. After this point, the file becomes eligible for removal when Compact is run. A value of 0 is possible, which means that previous versions do not need to be kept. You can enter a number of days here.

For example, you can enter 0 here to mean that after compaction runs, most previous versions of files are deleted. (Benefit: takes less disk space). Or, you can enter 365 days here to mean that even when Compact is run, only files that have been deleted for a year will be removed. (Benefit: keeps many versions of files).

#### Set approximate filesize for archive files

By default, the .tar files are roughly 64Mb each. The size can be adjusted here.

#### Set pause duration when running backups

When backing up files, we'll pause every thirty seconds, to be kind to the CPU and not drain your laptop battery. How long should we pause?

#### Set strength of data compaction

(An advanced option, the default should be fine).

When thorough compaction is run, what is the threshold of megabytes saved that says Compaction is worth doing? As an example, if this is set to '10', then when Compact is run, for each archive if at least 10 Mb of old versions can be removed, then the old data will be removed. But if only 5 Mb can be removed, it is skipped. In summary, if this number is smaller, Compaction will recover more space, at the expense of time taken and number of files written to disk.

#### Skip metadata changes

When this feature is enabled, changes in audio files that are solely metadata changes will not cause the entire file to be archived again. For example, if you frequently change the ID3 tags on your mp3 music, this can take up a lot of space because the entire file is re-added each time backups are run. (Within an mp3, ID3 tags precede the audio data).

With this feature enabled, we only backup the file when there are changes to the actual audio data. Supported for mp3, ogg, m4a, and flac files.

Prerequisite on Linux: install ffmpeg. (Use Google to find the right instructions for your distro)

Prerequisite on Windows: download static ffmpeg binaries, for example from [this site](https://ffmpeg.zeranoe.com/builds/). Unzip everything and copy ffmpeg.exe into glacial_backup's "tools" directory.

