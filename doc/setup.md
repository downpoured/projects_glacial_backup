
Setup | [Backup](backup.md) | [Restore](restore.md) | [Other features](other.md)

## Setting up GlacialBackup

### Windows

* download [glacial\_backup020win32.zip](https://github.com/downpoured/projects_glacial_backup/releases/download/v0.2.0/glacial_backup020win32.zip)

* extract all files from the .zip file

* run glacial_backup.exe

### Linux

Please open a terminal and run the following,

    sudo apt-get install xz-utils
    mkdir glacial_backup
    cd glacial_backup
    git clone https://github.com/downpoured/projects_glacial_backup.git
    cd projects_glacial_backup/src
    make ship
    ./glacial_backup.out

