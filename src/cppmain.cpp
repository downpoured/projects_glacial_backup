#include "util.h"
#include "util_os.h"
#include "sphash.h"
#include "user_config.h"
#include "structures.h"
#include "archive.h"
#include "metadata.h"
#include "operations.h"
#include "tests.h"

/*

Checksum of local index
	Uses file headers and runs a quick checksum.

Backup(bool fullScan, bool previewOnly)
	Get a list of all of the deleted whicharchives
	load all of the latest snapshot.000x.dat into map pathhash96+length32 contenthash128 modtime(32) snapshot(16)archive(16)  (40bytes)
	sort it to finalize
	then create a second map, contenthashJustThisSessionFirst32 --> ballpark index into first map (8 bytes)
	populate it by iterating through first map, then sort to finalize
	walk through filesystem:
		if not audio file:
			current.whichArchive = 0
			current.filepathhash = (compute filepathhash)
			current.modtime = (get modtime)
			current.length = (get length)
			bool filepathHashInMap = filepathhash in the map even if length differs
			if filepathHashInMap, and not fullScan, and if there's entry w length|modtime is the same
				current.contenthash = previous.contenthash
			else:
				current.contenthash = (compute hash)

			if row exists with contenthash same and length|(strict and modtime) the same, and archives not in deletedArchives
				current.whichArchive = thatrow.whichArchive
			else
				current.whichArchive = (add it; get new archive from archivebuilder)
			
			if not filepathHashInMap: write to filepaths.txt
			write current to disk

		else if audiofile
			current.whichArchive = 0
			current.filepathhash = (compute filepathhash)
			current.modtime = (get modtime)
			current.length = 0xffffff (possible to get audio length with ffmpeg but that's a bit slower)
			bool filepathHashInMap = filepathhash in the map
			if filepathHashInMap, and not fullScan, and if there's entry w modtime is the same
				current.length = previous.length
				current.contenthash = previous.contenthash
			else:
				current.length = (compute length)
				current.contenthash = (compute hash)
				need to write out the metadata :(

			if row exists with contenthash same and length|(strict and modtime) the same, and archives not in deletedArchives
				current.whichArchive = thatrow.whichArchive
			else
				current.whichArchive = (add it; get new archive from archivebuilder)

			if not filepathHashInMap: write to filepaths.txt
			write current to disk


Restore(choose any snapshot.0001.dat)
	Get a list of all of the deleted whicharchives
	load all of the latest snapshot.000x.dat into map pathhash96 --> modtime|length(32) contenthashJustThisSession96 snapshot(16)archive(16)  (32bytes)
	sort it to finalize
	we'll treat the snapshot(16)archive(16) as a way to flag to say we've done it, though. set it to all ones.
	walk through fullfilenames.7z filelists, backwards:
		if filepathhash+length+contenthash+everything is in map, and isn't checked off yet,
			if not in deletedarchives:
				write to csv file
				check this row off (writing null to it)

	create a text file latestAudioMetadata.txt
	walk through fullfilenames.7z audiodata, backwards:
		if filepathhash+length+contenthash+everything is in map, and isn't checked off yet,
			write to file latestAudioMetadata.txt
			check this row off (writing null to it)

Cull
	ask user for a cutoff time, from the snapshot's written dates / take from the ini file
	make a list of everything that must be kept alive, by loading that snapshot.00x.dat file
	make a map from whicharchive to nGood, nBad (using the same bsearch structure, preallocate by reading snapshot.00x headers which say how many archives there are)
	walk through the earliest snapshot.00x.dat files until the cutoff:
		if the path96, etc is in must-be-kept-alive, good++
		else, bad++
	list all the ones that are only bad. can also say percentage bad if it's really close.

	
Describe Contents
	(creates a csv file)
	Get a list of all of the deleted whicharchives
	choose a snapshot
	expand all of fullfilenames.7z filelists
	load all of the chosen snapshot.000x.dat into map pathhash96 --> modtime|length(32) contenthashJustThisSession96 snapshot(16)archive(16)  (32bytes)
	walk through fullfilenames.7z filelists, backwards:
		if filepathhash+length+contenthash+everything is in map, and isn't checked off yet,
			if not in deletedarchives:
				write to csv file
				check this row off (writing null to it)


Files Persisted
---------------------------------
snap_0000_archives_removed.ini
	manually user-edited text file, removed=ar_0000_0001,ar_0000_0002
snap_0000_audiotags.dat
	map from pathhash to audio metadata
snap_0000_filepaths.dat
	map from pathhash to filepath and contenthash (complete, for entire current user's state) and whicharchive
snap_0000_snapshot_index.dat
	map from pathhash to lastmodtime, length, contenthash


Structures in memory
---------------------------------------
Read each snap_0000_snapshot_index.dat and make two maps,
From pathhash to lastmodtime, length, contenthash, whicharchive
From contenthash to ptr in first map

*/


int MvException::_suppressDialogs = 0;
MvSimpleLogging g_logging;

int main(int argc, char *argv[])
{
	OS_PreventMultipleInstances prevent;
	if (prevent.AcquireAndCheckIfOtherInstanceIsRunning("downpoured_glacial_backup"))
	{
		printf("please run only one instance at a time.");
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

	printf("Complete.");
}
