#include "util.h"
#include "util_posix.h"
#include "sphash.h"
#include "util_archiver.h"
#include "data_structures.h"
#include "archive_builder.h"
#include "operations.h"
#include "tests.h"

/*

Welcome to projects_glacial_backup
Designed with the following in mind:
	File moves and renames are efficiently tracked
	Easily and safely remove old archives, or entire snapshots
	(it's as easy as deleting two files, the .dat file and the corresponding .7z file)
	The program does not run continuously in the background.
	Balance between a few large archives (good storage efficiency, but once uploaded, hard to remove obsolete data)
	and many small archives (easier to remove obsolete data, but worse storage efficiency+filesystem overhead)
	Optionally, common audio filetypes can track ID3 metadata tags separately from audio data, for greater efficiency.


A 'group' is a set of the user's directories, to be backed up together.
Every 'snapshot', containing the content of a group at a point in time, contains 
	- a list of all filepaths and their hashes
	- many 'archives'
Every 'archive' contains
	- an index with the file hashes in this archive
	- one or more 7z files containing content.
The total size will increase as more snapshots are made.
The user can choose to either keep all past revisions, or prune past revisions identified by the CheckForOldData tool.


Design 1:
	Convert all filenames to UTF8,
	have one 'worldlist.dat' file for each snapshot that has both filenames in utf8 and records, mapping filename to 
		(this file is used for both 1) restore and 2) mapping filepath+mod time to a hash to avoid having to rehash files
	have many 'archive_0000_0001.dat' files for each snapshot that represent an archive, contains no filepaths and essentially says which filehashes are in the archive
	load worldlist.dat into memory, can do this a bit more cheaply by loading the entire file into contiguous memory, and 
		then having a separate lookup from filepath hash to pointer into the contiguous memory.
	Cons: loading worldlist.dat will still be fairly expensive, reading about 10Mb from disk

Design 2:
	do not load all of worldlist.dat into memory, only the parts without the filenames.
	when restoring, we can load all of it into memory. but if we use strong enough hashes for the filepath,
	we should just use filepath hashes, and even if there is a collission the mod time also has to match
	Pros: the worldlist.dat file is a clear state of the world for each snapshot
	Cons: when reading worldlist.dat we still have to jump around a lot since records are unevenly sized
	Pros: we can still easily manually cull files from the archive by deleting its archive_0000_0001.dat
	Cons: realized that if there is a archive_0000_0001.dat for each archive, we'll need to read 1000s of these files...
	so maybe we can make archive_0000_0001.sentinel files that are never read, just check for existence?
	but even better to have a _removed ini for user to type it in, at least perf wise.

Design 3:
	during the backup phase, do not work with filepaths at all. only filepath hashes.
	also realized that using utf on windows might not be the best, it's safer to keep the utf16 representation for Restore.

	
Files Persisted
---------------------------------
snap_0000_archives_removed.ini
	manually user-edited text file, removed=ar_0000_0001,ar_0000_0002
snap_0000_audiotags.dat
	map from pathhash to audio metadata
snap_0000_filepaths.dat
	map from pathhash to filepath and contenthash (complete, for entire current user's state)
snap_0000_snapshot_index.dat
	map from pathhash to lastmodtime, length, contenthash, whicharchive


Structures in memory
---------------------------------------
Read each snap_0000_snapshot_index.dat and make two maps,
From pathhash to lastmodtime, length, contenthash, whicharchive
From contenthash to ptr in first map
Backup
	Read all snap_0000_snapshot_index.dat, from earliest forward, 
		replacing old data with newly recieved data
		ignoring anything where the whicharchive points to an archive that has been deleted.
		then create the contenthash to ptr-in-first map structure and finalize it.
	Walk through all files
		If pathhash found in pathhash structure and, iterate until seeing if there's an entry with modifiedtime is the same and length is the same,
			contenthash = record.contenthash
			whicharchive = record.whicharchive
			
		else hash the contents of the file to compute the contenthash
			if this is an audio file, write the pathhash and contenthash and audiotags to audiotags.dat
			lastmodtime = (get from file)
			length = (get from file)
			contenthash = (compute the hash)
			if contenthash is in the map from contenthash to ptr-in-first map structure
				whicharchive = thatrecord.whicharchive
			else 
				if contenthash is in mapCreatedThisSession
					whicharchive = thatrecord.whicharchive
				else
					whicharchive = (newly created archive)
					archivecreator.add file to archive
				write out record to disk from pathhash to lastmodtime, length, contenthash, whicharchive
		write out record to disk for map from pathhash to filepath and contenthash, add padding to the string for alignment
	flush archivecreator

Findculling
	Read the complete filelists, from earliest forward.
		but only look at the contenthashes. make map of contenthash to last-snapshot-seen.
	howGoodIsEachArchive is std::vector<std::tuple< lastTimeSeen, nb4cutoff, ntotal, snapshotarchivenumber >>
	for completion mode, where we want to see status of all archives, just say that the cutoff is MAXINT
	go through the filelist.dat, from latest meeting the criteria backward
		map[]
		
	Read all snap_0000_snapshot_index.dat, 
		Read all entries one at a time
			look at last-snapshot-seen. 
			if last-snapshot-seen is 
			change the lastTimeSeen and ntotal and so on accordingly.
	sort the vector (actually use a FastReadonlyMultimap)
	tell the user what looks bad

Restore
	list possible filepaths.dat and have user choose one of them (defaults to the latest)
	go through the filepath.dat (until chosen snapshot) maps to which-archive, so can expand that one if not already expanded, and get the filecontents
	if need audiotags:
		algorithm has 2x the disk io but way less memory usage !
		new map whichAudioTagsToPrint from filepathhash to latestSeen
		read all through the one filepaths.dat and mark what files are seen in whichAudioTagsToPrint. whichAudioTagsToPrint=1(ready)
		read through all of the audiotags (backwards from chosen snapshot) 
			if in whichAudioTagsToPrint==ready then set whichAudioTagsToPrint=this file
		read through all of the audiotags (backwards from chosen snapshot)
			if whichAudioTagsToPrint[filepathhash]==this file: print it out!

*/

int MvException::_suppressDialogs = 0;
MvSimpleLogging g_logging;

int main(int argc, char *argv[])
{
	OS_PreventMultipleInstances prevent;
	if (prevent.AcquireAndCheckIfOtherInstanceIsRunning("downpoured_glacial_backup"))
	{
		puts("please run only one instance at a time.");
		return 0;
	}
	try
	{
		Tests_AllTests();
#if 0
		context = "reading configuration";
		BackupConfigurationGeneral generalSettings;
		std::vector<BackupConfiguration> configurations;
		BackupConfiguration::ParseFromConfigFile("config.ini", generalSettings, configurations);
		
		context = "loading data";
		LogFile generalLog(L"generallog.txt");
		for (auto it = configurations.begin(); it != configurations.end(); ++it)
		{
			generalLog.WriteLine(L"beginning load for group ", it->m_groupname.c_str());
			FilesSeenInGroup filesSeen;
			filesSeen.LoadFromDisk(it->m_logpath.c_str(), it->m_groupname.c_str(), generalLog);
		
			RunArchiveContext context(filesSeen, it->m_logpath.c_str(), it->m_arDirs, it->m_disregardTaggingChangesInAudioFiles, generalLog);
			RunArchive(context);
			filesSeen.PersistToDisk(it->m_logpath.c_str(), it->m_groupname.c_str(), generalLog);
		}
#endif
	}
	catch (std::exception& e)
	{
		alertdialog("uncaught exception");
		printf("exception occurred, %s\n", e.what());
	}
	
	puts("Complete.");
}
