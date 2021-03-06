/*-------------------------------------------------------------------------
 *
 * xlogutils.c
 *
 * PostgreSQL transaction log manager utility routines
 *
 * This file contains support routines that are used by XLOG replay functions.
 * None of this code is used during normal system operation.
 *
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/backend/access/transam/xlogutils.c,v 1.66 2009/01/01 17:23:36 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "access/xlogutils.h"
#include "catalog/catalog.h"
#include "storage/bufmgr.h"
#include "storage/smgr.h"
#include "utils/guc.h"
#include "utils/hsearch.h"
#include "utils/rel.h"

#include "cdb/cdbpersistentrecovery.h"
#include "cdb/cdbpersistenttablespace.h"


/*
 * During XLOG replay, we may see XLOG records for incremental updates of
 * pages that no longer exist, because their relation was later dropped or
 * truncated.  (Note: this is only possible when full_page_writes = OFF,
 * since when it's ON, the first reference we see to a page should always
 * be a full-page rewrite not an incremental update.)  Rather than simply
 * ignoring such records, we make a note of the referenced page, and then
 * complain if we don't actually see a drop or truncate covering the page
 * later in replay.
 */
typedef struct xl_invalid_page_key
{
	RelFileNode node;			/* the relation */
	ForkNumber	forkno;			/* the fork number */
	BlockNumber blkno;			/* the page */
} xl_invalid_page_key;

typedef struct xl_invalid_page
{
	xl_invalid_page_key key;	/* hash key ... must be first */
	bool		present;		/* page existed but contained zeroes */
} xl_invalid_page;

static HTAB *invalid_page_tab = NULL;


/* Log a reference to an invalid page */
static void
log_invalid_page(RelFileNode node, ForkNumber forkno, BlockNumber blkno,
				 bool present)
{
	xl_invalid_page_key key;
	xl_invalid_page *hentry;
	bool		found;

	/*
	 * Log references to invalid pages at DEBUG1 level.  This allows some
	 * tracing of the cause (note the elog context mechanism will tell us
	 * something about the XLOG record that generated the reference).
	 */
	if (log_min_messages <= DEBUG1 || client_min_messages <= DEBUG1)
	{
		char *path = relpath(node, forkno);
		if (present)
		{
			elog(DEBUG1, "page %u of relation %s is uninitialized",
				 blkno, path);
			if (Debug_persistent_recovery_print)
				elog(PersistentRecovery_DebugPrintLevel(),
					 "log_invalid_page: page %u of relation %s is uninitialized",
					 blkno, path);
		}
		else
		{
			elog(DEBUG1, "page %u of relation %s does not exist",
				 blkno, path);
			if (Debug_persistent_recovery_print)
				elog(PersistentRecovery_DebugPrintLevel(),
					 "log_invalid_page: page %u of relation %s does not exist",
					 blkno, path);
		}
		pfree(path);
	}

	if (invalid_page_tab == NULL)
	{
		/* create hash table when first needed */
		HASHCTL		ctl;

		memset(&ctl, 0, sizeof(ctl));
		ctl.keysize = sizeof(xl_invalid_page_key);
		ctl.entrysize = sizeof(xl_invalid_page);
		ctl.hash = tag_hash;

		invalid_page_tab = hash_create("XLOG invalid-page table",
									   100,
									   &ctl,
									   HASH_ELEM | HASH_FUNCTION);
	}

	/* we currently assume xl_invalid_page_key contains no padding */
	key.node = node;
	key.forkno = forkno;
	key.blkno = blkno;
	hentry = (xl_invalid_page *)
		hash_search(invalid_page_tab, (void *) &key, HASH_ENTER, &found);

	if (!found)
	{
		/* hash_search already filled in the key */
		hentry->present = present;
	}
	else
	{
		/* repeat reference ... leave "present" as it was */
	}
}

/* Forget any invalid pages >= minblkno, because they've been dropped */
static void
forget_invalid_pages(RelFileNode node, ForkNumber forkno, BlockNumber minblkno)
{
	HASH_SEQ_STATUS status;
	xl_invalid_page *hentry;

	if (invalid_page_tab == NULL)
		return;					/* nothing to do */

	hash_seq_init(&status, invalid_page_tab);

	while ((hentry = (xl_invalid_page *) hash_seq_search(&status)) != NULL)
	{
		if (RelFileNodeEquals(hentry->key.node, node) &&
			hentry->key.forkno == forkno &&
			hentry->key.blkno >= minblkno)
		{
			if (log_min_messages <= DEBUG2 || client_min_messages <= DEBUG2)
			{
				char *path = relpath(hentry->key.node, forkno);
				elog(DEBUG2, "page %u of relation %s has been dropped",
					 hentry->key.blkno, path);
				if (Debug_persistent_recovery_print)
					elog(PersistentRecovery_DebugPrintLevel(),
						 "forget_invalid_pages: page %u of relation %s has been dropped",
						 hentry->key.blkno, path);
				pfree(path);
			}

			if (hash_search(invalid_page_tab,
							(void *) &hentry->key,
							HASH_REMOVE, NULL) == NULL)
				elog(ERROR, "hash table corrupted");
		}
	}
}

#if 0 /* XLOG_DBASE_DROP is not used in GPDB so this function will never get called */
/* Forget any invalid pages in a whole database */
static void
forget_invalid_pages_db(Oid tblspc, Oid dbid)
{
	HASH_SEQ_STATUS status;
	xl_invalid_page *hentry;

	if (invalid_page_tab == NULL)
		return;					/* nothing to do */

	hash_seq_init(&status, invalid_page_tab);

	while ((hentry = (xl_invalid_page *) hash_seq_search(&status)) != NULL)
	{
		if ((!OidIsValid(tblspc) || hentry->key.node.spcNode == tblspc) &&
			hentry->key.node.dbNode == dbid)
		{
			if (log_min_messages <= DEBUG2 || client_min_messages <= DEBUG2)
			{
				char *path = relpath(hentry->key.node, hentry->key.forkno);
				elog(DEBUG2, "page %u of relation %s has been dropped",
					 hentry->key.blkno, path);
				if (Debug_persistent_recovery_print)
					elog(PersistentRecovery_DebugPrintLevel(),
						 "forget_invalid_pages_db: %u of relation %s has been dropped",
						 hentry->key.blkno, path);
				pfree(path);
			}

			if (hash_search(invalid_page_tab,
							(void *) &hentry->key,
							HASH_REMOVE, NULL) == NULL)
				elog(ERROR, "hash table corrupted");
		}
	}
}
#endif

#ifdef USE_SEGWALREP
/* Forget an invalid AO/AOCO segment file */
static void
forget_invalid_segment_file(RelFileNode rnode, uint32 segmentFileNum)
{
	xl_invalid_page_key key;
	bool		found;

	if (invalid_page_tab == NULL)
		return;					/* nothing to do */

	key.node = rnode;
	key.forkno = MAIN_FORKNUM;
	key.blkno = segmentFileNum;
	hash_search(invalid_page_tab,
				(void *) &key,
				HASH_FIND, &found);
	if (!found)
		return;

	if (hash_search(invalid_page_tab,
					(void *) &key,
					HASH_REMOVE, &found) == NULL)
		elog(ERROR, "hash table corrupted");

	elog(Debug_persistent_recovery_print ? PersistentRecovery_DebugPrintLevel() : DEBUG2,
		 "segmentfile %u of relation %u/%u/%u has been dropped",
		 key.blkno, key.node.spcNode,
		 key.node.dbNode, key.node.relNode);
}
#endif

/* Complain about any remaining invalid-page entries */
void
XLogCheckInvalidPages(void)
{
	HASH_SEQ_STATUS status;
	xl_invalid_page *hentry;
	bool		foundone = false;

	if (invalid_page_tab == NULL)
		return;					/* nothing to do */

	hash_seq_init(&status, invalid_page_tab);

	/*
	 * Our strategy is to emit WARNING messages for all remaining entries and
	 * only PANIC after we've dumped all the available info.
	 */
	while ((hentry = (xl_invalid_page *) hash_seq_search(&status)) != NULL)
	{
		char *path = relpath(hentry->key.node, hentry->key.forkno);
		if (hentry->present)
			elog(WARNING, "page %u of relation %s was uninitialized",
				 hentry->key.blkno, path);
		else
			elog(WARNING, "page %u of relation %s did not exist",
				 hentry->key.blkno, path);
		pfree(path);
		foundone = true;
	}

	if (foundone)
		elog(PANIC, "WAL contains references to invalid pages");

	hash_destroy(invalid_page_tab);
	invalid_page_tab = NULL;
}

/*
 * XLogReadBuffer
 *		A shorthand of XLogReadBufferExtended(), for reading from the main
 *		fork.
 *
 * For historical reasons, instead of a ReadBufferMode argument, this only
 * supports RBM_ZERO (init == true) and RBM_NORMAL (init == false) modes.
 */
Buffer
XLogReadBuffer(RelFileNode rnode, BlockNumber blkno, bool init)
{
	return XLogReadBufferExtended(rnode, MAIN_FORKNUM, blkno,
								  init ? RBM_ZERO : RBM_NORMAL);
}

/*
 * XLogReadBufferExtended
 *		Read a page during XLOG replay
 *
 * This is functionally comparable to ReadBuffer followed by
 * LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE): you get back a pinned
 * and locked buffer.  (Getting the lock is not really necessary, since we
 * expect that this is only used during single-process XLOG replay, but
 * some subroutines such as MarkBufferDirty will complain if we don't.)
 *
 * There's some differences in the behavior wrt. the "mode" argument,
 * compared to ReadBufferExtended:
 *
 * In RBM_NORMAL mode, if the page doesn't exist, or contains all-zeroes, we
 * return InvalidBuffer. In this case the caller should silently skip the
 * update on this page. (In this situation, we expect that the page was later
 * dropped or truncated. If we don't see evidence of that later in the WAL
 * sequence, we'll complain at the end of WAL replay.)
 *
 * In RBM_ZERO and RBM_ZERO_ON_ERROR modes, if the page doesn't exist, the
 * relation is extended with all-zeroes pages up to the given block number.
 */
Buffer
XLogReadBufferExtended(RelFileNode rnode, ForkNumber forknum,
					   BlockNumber blkno, ReadBufferMode mode)
{
	BlockNumber lastblock;
	Buffer		buffer;
	SMgrRelation smgr;

	if (forknum == MAIN_FORKNUM)
		MIRROREDLOCK_BUFMGR_MUST_ALREADY_BE_HELD;

	Assert(blkno != P_NEW);

	/* Open the relation at smgr level */
	smgr = smgropen(rnode);

	/*
	 * Create the target file if it doesn't already exist.  This lets us cope
	 * if the replay sequence contains writes to a relation that is later
	 * deleted.  (The original coding of this routine would instead suppress
	 * the writes, but that seems like it risks losing valuable data if the
	 * filesystem loses an inode during a crash.  Better to write the data
	 * until we are actually told to delete the file.)
	 */

	/*
	 * The multi-pass WAL replay during crash recovery in GPDB, guided by
	 * persistent tables, may make the following smgrcreate() call redundant.
	 * The problem is shortlived, it will go away once filerep and persistent
	 * tables are removed.  To be clear, that the smgrcreate() call exists in
	 * upstream.
	 */
	MirrorDataLossTrackingState mirrorDataLossTrackingState;
	int64 mirrorDataLossTrackingSessionNum;
	bool mirrorDataLossOccurred;

	// UNDONE: What about the persistent rel files table???
	// UNDONE: This condition should not occur anymore.
	// UNDONE: segmentFileNum and AO?
	if (forknum ==  MAIN_FORKNUM)
	{
		mirrorDataLossTrackingState =
			FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
				&mirrorDataLossTrackingSessionNum);

		smgrmirroredcreate(smgr,
						   /* relationName */ NULL,    // Ok to be NULL -- we don't know the name here.
						   mirrorDataLossTrackingState,
						   mirrorDataLossTrackingSessionNum,
						   /* ignoreAlreadyExists */ true,
						   &mirrorDataLossOccurred);
	}
	else
		smgrcreate(smgr, forknum, true);

	lastblock = smgrnblocks(smgr, forknum);

	if (blkno < lastblock)
	{
		/* page exists in file */
		buffer = ReadBufferWithoutRelcache(rnode, false, forknum, blkno,
										   mode, NULL);
	}
	else
	{
		/* hm, page doesn't exist in file */
		if (mode == RBM_NORMAL)
		{
			log_invalid_page(rnode, forknum, blkno, false);
			return InvalidBuffer;
		}
		/* OK to extend the file */
		/* we do this in recovery only - no rel-extension lock needed */
		Assert(InRecovery);
		buffer = InvalidBuffer;
		while (blkno >= lastblock)
		{
			if (buffer != InvalidBuffer)
				ReleaseBuffer(buffer);
			buffer = ReadBufferWithoutRelcache(rnode, false, forknum,
											   P_NEW, mode, NULL);
			lastblock++;
		}
		Assert(BufferGetBlockNumber(buffer) == blkno);
	}

	LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);

	if (mode == RBM_NORMAL)
	{
		/* check that page has been initialized */
		Page		page = (Page) BufferGetPage(buffer);

		if (PageIsNew(page))
		{
			UnlockReleaseBuffer(buffer);
			log_invalid_page(rnode, forknum, blkno, true);
			return InvalidBuffer;
		}
	}

	return buffer;
}

#ifdef USE_SEGWALREP
/*
 * If the AO segment file does not exist, log the relfilenode into the
 * invalid_page_table hash table using the segment file number as the
 * block number to avoid creating a new hash table.  The entry will be
 * removed if there is a following MMXLOG_REMOVE_FILE record for the
 * relfilenode.
 */
void
XLogAOSegmentFile(RelFileNode rnode, uint32 segmentFileNum)
{
	log_invalid_page(rnode, MAIN_FORKNUM, segmentFileNum, false);
}
#endif

/*
 * Struct actually returned by XLogFakeRelcacheEntry, though the declared
 * return type is Relation.
 */
typedef struct
{
	RelationData		reldata;	/* Note: this must be first */
	FormData_pg_class	pgc;
} FakeRelCacheEntryData;

typedef FakeRelCacheEntryData *FakeRelCacheEntry;

/*
 * Create a fake relation cache entry for a physical relation
 *
 * It's often convenient to use the same functions in XLOG replay as in the
 * main codepath, but those functions typically work with a relcache entry. 
 * We don't have a working relation cache during XLOG replay, but this 
 * function can be used to create a fake relcache entry instead. Only the 
 * fields related to physical storage, like rd_rel, are initialized, so the 
 * fake entry is only usable in low-level operations like ReadBuffer().
 *
 * Caller must free the returned entry with FreeFakeRelcacheEntry().
 */
Relation
CreateFakeRelcacheEntry(RelFileNode rnode)
{
	FakeRelCacheEntry fakeentry;
	Relation rel;

	/* Allocate the Relation struct and all related space in one block. */
	fakeentry = palloc0(sizeof(FakeRelCacheEntryData));
	rel = (Relation) fakeentry;

	rel->rd_rel = &fakeentry->pgc;
	rel->rd_node = rnode;

	/* We don't know the name of the relation; use relfilenode instead */
	sprintf(RelationGetRelationName(rel), "%u", rnode.relNode);

	/*
	 * We set up the lockRelId in case anything tries to lock the dummy
	 * relation.  Note that this is fairly bogus since relNode may be
	 * different from the relation's OID.  It shouldn't really matter
	 * though, since we are presumably running by ourselves and can't have
	 * any lock conflicts ...
	 */
	rel->rd_lockInfo.lockRelId.dbId = rnode.dbNode;
	rel->rd_lockInfo.lockRelId.relId = rnode.relNode;

	rel->rd_targblock = InvalidBlockNumber;
	rel->rd_fsm_nblocks = InvalidBlockNumber;
	rel->rd_vm_nblocks = InvalidBlockNumber;
	rel->rd_smgr = NULL;

	return rel;
}

/*
 * Free a fake relation cache entry.
 */
void
FreeFakeRelcacheEntry(Relation fakerel)
{
	pfree(fakerel);
}

/*
 * Drop a relation during XLOG replay
 *
 * This is called when the relation is about to be deleted; we need to remove
 * any open "invalid-page" records for the relation.
 */
void
XLogDropRelation(RelFileNode rnode, ForkNumber forknum)
{
	forget_invalid_pages(rnode, forknum, 0);
}

#ifdef USE_SEGWALREP
/* Drop an AO/CO segment file from the invalid_page_tab hash table */
void
XLogAODropSegmentFile(RelFileNode rnode, uint32 segmentFileNum)
{
	forget_invalid_segment_file(rnode, segmentFileNum);
}
#endif

#if 0 /* XLOG_DBASE_DROP is not used in GPDB so this function will never get called */
/*
 * Drop a whole database during XLOG replay
 *
 * As above, but for DROP DATABASE instead of dropping a single rel
 */
void
XLogDropDatabase(Oid tblspc, Oid dbid)
{
	/*
	 * This is unnecessarily heavy-handed, as it will close SMgrRelation
	 * objects for other databases as well. DROP DATABASE occurs seldom
	 * enough that it's not worth introducing a variant of smgrclose for
	 * just this purpose. XXX: Or should we rather leave the smgr entries
	 * dangling?
	 */
	smgrcloseall();

	forget_invalid_pages_db(tblspc, dbid);
}
#endif

/*
 * Truncate a relation during XLOG replay
 *
 * We need to clean up any open "invalid-page" records for the dropped pages.
 */
void
XLogTruncateRelation(RelFileNode rnode, ForkNumber forkNum,
					 BlockNumber nblocks)
{
	forget_invalid_pages(rnode, forkNum, nblocks);
}
