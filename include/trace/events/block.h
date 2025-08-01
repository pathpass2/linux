/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM block

#if !defined(_TRACE_BLOCK_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_BLOCK_H

#include <linux/blktrace_api.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/tracepoint.h>
#include <uapi/linux/ioprio.h>

#define RWBS_LEN	10

#define IOPRIO_CLASS_STRINGS \
	{ IOPRIO_CLASS_NONE,	"none" }, \
	{ IOPRIO_CLASS_RT,	"rt" }, \
	{ IOPRIO_CLASS_BE,	"be" }, \
	{ IOPRIO_CLASS_IDLE,	"idle" }, \
	{ IOPRIO_CLASS_INVALID,	"invalid"}

#ifdef CONFIG_BUFFER_HEAD
DECLARE_EVENT_CLASS(block_buffer,

	TP_PROTO(struct buffer_head *bh),

	TP_ARGS(bh),

	TP_STRUCT__entry (
		__field(  dev_t,	dev			)
		__field(  sector_t,	sector			)
		__field(  size_t,	size			)
	),

	TP_fast_assign(
		__entry->dev		= bh->b_bdev->bd_dev;
		__entry->sector		= bh->b_blocknr;
		__entry->size		= bh->b_size;
	),

	TP_printk("%d,%d sector=%llu size=%zu",
		MAJOR(__entry->dev), MINOR(__entry->dev),
		(unsigned long long)__entry->sector, __entry->size
	)
);

/**
 * block_touch_buffer - mark a buffer accessed
 * @bh: buffer_head being touched
 *
 * Called from touch_buffer().
 */
DEFINE_EVENT(block_buffer, block_touch_buffer,

	TP_PROTO(struct buffer_head *bh),

	TP_ARGS(bh)
);

/**
 * block_dirty_buffer - mark a buffer dirty
 * @bh: buffer_head being dirtied
 *
 * Called from mark_buffer_dirty().
 */
DEFINE_EVENT(block_buffer, block_dirty_buffer,

	TP_PROTO(struct buffer_head *bh),

	TP_ARGS(bh)
);
#endif /* CONFIG_BUFFER_HEAD */

/**
 * block_rq_requeue - place block IO request back on a queue
 * @rq: block IO operation request
 *
 * The block operation request @rq is being placed back into queue
 * @q.  For some reason the request was not completed and needs to be
 * put back in the queue.
 */
TRACE_EVENT(block_rq_requeue,

	TP_PROTO(struct request *rq),

	TP_ARGS(rq),

	TP_STRUCT__entry(
		__field(  dev_t,	dev			)
		__field(  sector_t,	sector			)
		__field(  unsigned int,	nr_sector		)
		__field(  unsigned short, ioprio		)
		__array(  char,		rwbs,	RWBS_LEN	)
		__dynamic_array( char,	cmd,	1		)
	),

	TP_fast_assign(
		__entry->dev	   = rq->q->disk ? disk_devt(rq->q->disk) : 0;
		__entry->sector    = blk_rq_trace_sector(rq);
		__entry->nr_sector = blk_rq_trace_nr_sectors(rq);
		__entry->ioprio    = req_get_ioprio(rq);

		blk_fill_rwbs(__entry->rwbs, rq->cmd_flags);
		__get_str(cmd)[0] = '\0';
	),

	TP_printk("%d,%d %s (%s) %llu + %u %s,%u,%u [%d]",
		  MAJOR(__entry->dev), MINOR(__entry->dev),
		  __entry->rwbs, __get_str(cmd),
		  (unsigned long long)__entry->sector, __entry->nr_sector,
		  __print_symbolic(IOPRIO_PRIO_CLASS(__entry->ioprio),
				   IOPRIO_CLASS_STRINGS),
		  IOPRIO_PRIO_HINT(__entry->ioprio),
		  IOPRIO_PRIO_LEVEL(__entry->ioprio),  0)
);

DECLARE_EVENT_CLASS(block_rq_completion,

	TP_PROTO(struct request *rq, blk_status_t error, unsigned int nr_bytes),

	TP_ARGS(rq, error, nr_bytes),

	TP_STRUCT__entry(
		__field(  dev_t,	dev			)
		__field(  sector_t,	sector			)
		__field(  unsigned int,	nr_sector		)
		__field(  int	,	error			)
		__field(  unsigned short, ioprio		)
		__array(  char,		rwbs,	RWBS_LEN	)
		__dynamic_array( char,	cmd,	1		)
	),

	TP_fast_assign(
		__entry->dev	   = rq->q->disk ? disk_devt(rq->q->disk) : 0;
		__entry->sector    = blk_rq_pos(rq);
		__entry->nr_sector = nr_bytes >> 9;
		__entry->error     = blk_status_to_errno(error);
		__entry->ioprio    = req_get_ioprio(rq);

		blk_fill_rwbs(__entry->rwbs, rq->cmd_flags);
		__get_str(cmd)[0] = '\0';
	),

	TP_printk("%d,%d %s (%s) %llu + %u %s,%u,%u [%d]",
		  MAJOR(__entry->dev), MINOR(__entry->dev),
		  __entry->rwbs, __get_str(cmd),
		  (unsigned long long)__entry->sector, __entry->nr_sector,
		  __print_symbolic(IOPRIO_PRIO_CLASS(__entry->ioprio),
				   IOPRIO_CLASS_STRINGS),
		  IOPRIO_PRIO_HINT(__entry->ioprio),
		  IOPRIO_PRIO_LEVEL(__entry->ioprio), __entry->error)
);

/**
 * block_rq_complete - block IO operation completed by device driver
 * @rq: block operations request
 * @error: status code
 * @nr_bytes: number of completed bytes
 *
 * The block_rq_complete tracepoint event indicates that some portion
 * of operation request has been completed by the device driver.  If
 * the @rq->bio is %NULL, then there is absolutely no additional work to
 * do for the request. If @rq->bio is non-NULL then there is
 * additional work required to complete the request.
 */
DEFINE_EVENT(block_rq_completion, block_rq_complete,

	TP_PROTO(struct request *rq, blk_status_t error, unsigned int nr_bytes),

	TP_ARGS(rq, error, nr_bytes)
);

/**
 * block_rq_error - block IO operation error reported by device driver
 * @rq: block operations request
 * @error: status code
 * @nr_bytes: number of completed bytes
 *
 * The block_rq_error tracepoint event indicates that some portion
 * of operation request has failed as reported by the device driver.
 */
DEFINE_EVENT(block_rq_completion, block_rq_error,

	TP_PROTO(struct request *rq, blk_status_t error, unsigned int nr_bytes),

	TP_ARGS(rq, error, nr_bytes)
);

DECLARE_EVENT_CLASS(block_rq,

	TP_PROTO(struct request *rq),

	TP_ARGS(rq),

	TP_STRUCT__entry(
		__field(  dev_t,	dev			)
		__field(  sector_t,	sector			)
		__field(  unsigned int,	nr_sector		)
		__field(  unsigned int,	bytes			)
		__field(  unsigned short, ioprio		)
		__array(  char,		rwbs,	RWBS_LEN	)
		__array(  char,         comm,   TASK_COMM_LEN   )
		__dynamic_array( char,	cmd,	1		)
	),

	TP_fast_assign(
		__entry->dev	   = rq->q->disk ? disk_devt(rq->q->disk) : 0;
		__entry->sector    = blk_rq_trace_sector(rq);
		__entry->nr_sector = blk_rq_trace_nr_sectors(rq);
		__entry->bytes     = blk_rq_bytes(rq);
		__entry->ioprio	   = req_get_ioprio(rq);

		blk_fill_rwbs(__entry->rwbs, rq->cmd_flags);
		__get_str(cmd)[0] = '\0';
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
	),

	TP_printk("%d,%d %s %u (%s) %llu + %u %s,%u,%u [%s]",
		  MAJOR(__entry->dev), MINOR(__entry->dev),
		  __entry->rwbs, __entry->bytes, __get_str(cmd),
		  (unsigned long long)__entry->sector, __entry->nr_sector,
		  __print_symbolic(IOPRIO_PRIO_CLASS(__entry->ioprio),
				   IOPRIO_CLASS_STRINGS),
		  IOPRIO_PRIO_HINT(__entry->ioprio),
		  IOPRIO_PRIO_LEVEL(__entry->ioprio), __entry->comm)
);

/**
 * block_rq_insert - insert block operation request into queue
 * @rq: block IO operation request
 *
 * Called immediately before block operation request @rq is inserted
 * into queue @q.  The fields in the operation request @rq struct can
 * be examined to determine which device and sectors the pending
 * operation would access.
 */
DEFINE_EVENT(block_rq, block_rq_insert,

	TP_PROTO(struct request *rq),

	TP_ARGS(rq)
);

/**
 * block_rq_issue - issue pending block IO request operation to device driver
 * @rq: block IO operation request
 *
 * Called when block operation request @rq from queue @q is sent to a
 * device driver for processing.
 */
DEFINE_EVENT(block_rq, block_rq_issue,

	TP_PROTO(struct request *rq),

	TP_ARGS(rq)
);

/**
 * block_rq_merge - merge request with another one in the elevator
 * @rq: block IO operation request
 *
 * Called when block operation request @rq from queue @q is merged to another
 * request queued in the elevator.
 */
DEFINE_EVENT(block_rq, block_rq_merge,

	TP_PROTO(struct request *rq),

	TP_ARGS(rq)
);

/**
 * block_io_start - insert a request for execution
 * @rq: block IO operation request
 *
 * Called when block operation request @rq is queued for execution
 */
DEFINE_EVENT(block_rq, block_io_start,

	TP_PROTO(struct request *rq),

	TP_ARGS(rq)
);

/**
 * block_io_done - block IO operation request completed
 * @rq: block IO operation request
 *
 * Called when block operation request @rq is completed
 */
DEFINE_EVENT(block_rq, block_io_done,

	TP_PROTO(struct request *rq),

	TP_ARGS(rq)
);

/**
 * block_bio_complete - completed all work on the block operation
 * @q: queue holding the block operation
 * @bio: block operation completed
 *
 * This tracepoint indicates there is no further work to do on this
 * block IO operation @bio.
 */
TRACE_EVENT(block_bio_complete,

	TP_PROTO(struct request_queue *q, struct bio *bio),

	TP_ARGS(q, bio),

	TP_STRUCT__entry(
		__field( dev_t,		dev		)
		__field( sector_t,	sector		)
		__field( unsigned,	nr_sector	)
		__field( int,		error		)
		__array( char,		rwbs,	RWBS_LEN)
	),

	TP_fast_assign(
		__entry->dev		= bio_dev(bio);
		__entry->sector		= bio->bi_iter.bi_sector;
		__entry->nr_sector	= bio_sectors(bio);
		__entry->error		= blk_status_to_errno(bio->bi_status);
		blk_fill_rwbs(__entry->rwbs, bio->bi_opf);
	),

	TP_printk("%d,%d %s %llu + %u [%d]",
		  MAJOR(__entry->dev), MINOR(__entry->dev), __entry->rwbs,
		  (unsigned long long)__entry->sector,
		  __entry->nr_sector, __entry->error)
);

DECLARE_EVENT_CLASS(block_bio,

	TP_PROTO(struct bio *bio),

	TP_ARGS(bio),

	TP_STRUCT__entry(
		__field( dev_t,		dev			)
		__field( sector_t,	sector			)
		__field( unsigned int,	nr_sector		)
		__array( char,		rwbs,	RWBS_LEN	)
		__array( char,		comm,	TASK_COMM_LEN	)
	),

	TP_fast_assign(
		__entry->dev		= bio_dev(bio);
		__entry->sector		= bio->bi_iter.bi_sector;
		__entry->nr_sector	= bio_sectors(bio);
		blk_fill_rwbs(__entry->rwbs, bio->bi_opf);
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
	),

	TP_printk("%d,%d %s %llu + %u [%s]",
		  MAJOR(__entry->dev), MINOR(__entry->dev), __entry->rwbs,
		  (unsigned long long)__entry->sector,
		  __entry->nr_sector, __entry->comm)
);

/**
 * block_bio_backmerge - merging block operation to the end of an existing operation
 * @bio: new block operation to merge
 *
 * Merging block request @bio to the end of an existing block request.
 */
DEFINE_EVENT(block_bio, block_bio_backmerge,
	TP_PROTO(struct bio *bio),
	TP_ARGS(bio)
);

/**
 * block_bio_frontmerge - merging block operation to the beginning of an existing operation
 * @bio: new block operation to merge
 *
 * Merging block IO operation @bio to the beginning of an existing block request.
 */
DEFINE_EVENT(block_bio, block_bio_frontmerge,
	TP_PROTO(struct bio *bio),
	TP_ARGS(bio)
);

/**
 * block_bio_queue - putting new block IO operation in queue
 * @bio: new block operation
 *
 * About to place the block IO operation @bio into queue @q.
 */
DEFINE_EVENT(block_bio, block_bio_queue,
	TP_PROTO(struct bio *bio),
	TP_ARGS(bio)
);

/**
 * block_getrq - get a free request entry in queue for block IO operations
 * @bio: pending block IO operation (can be %NULL)
 *
 * A request struct has been allocated to handle the block IO operation @bio.
 */
DEFINE_EVENT(block_bio, block_getrq,
	TP_PROTO(struct bio *bio),
	TP_ARGS(bio)
);

/**
 * blk_zone_append_update_request_bio - update bio sector after zone append
 * @rq: the completed request that sets the bio sector
 *
 * Update the bio's bi_sector after a zone append command has been completed.
 */
DEFINE_EVENT(block_rq, blk_zone_append_update_request_bio,
	     TP_PROTO(struct request *rq),
	     TP_ARGS(rq)
);

/**
 * block_plug - keep operations requests in request queue
 * @q: request queue to plug
 *
 * Plug the request queue @q.  Do not allow block operation requests
 * to be sent to the device driver. Instead, accumulate requests in
 * the queue to improve throughput performance of the block device.
 */
TRACE_EVENT(block_plug,

	TP_PROTO(struct request_queue *q),

	TP_ARGS(q),

	TP_STRUCT__entry(
		__array( char,		comm,	TASK_COMM_LEN	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
	),

	TP_printk("[%s]", __entry->comm)
);

DECLARE_EVENT_CLASS(block_unplug,

	TP_PROTO(struct request_queue *q, unsigned int depth, bool explicit),

	TP_ARGS(q, depth, explicit),

	TP_STRUCT__entry(
		__field( int,		nr_rq			)
		__array( char,		comm,	TASK_COMM_LEN	)
	),

	TP_fast_assign(
		__entry->nr_rq = depth;
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
	),

	TP_printk("[%s] %d", __entry->comm, __entry->nr_rq)
);

/**
 * block_unplug - release of operations requests in request queue
 * @q: request queue to unplug
 * @depth: number of requests just added to the queue
 * @explicit: whether this was an explicit unplug, or one from schedule()
 *
 * Unplug request queue @q because device driver is scheduled to work
 * on elements in the request queue.
 */
DEFINE_EVENT(block_unplug, block_unplug,

	TP_PROTO(struct request_queue *q, unsigned int depth, bool explicit),

	TP_ARGS(q, depth, explicit)
);

/**
 * block_split - split a single bio struct into two bio structs
 * @bio: block operation being split
 * @new_sector: The starting sector for the new bio
 *
 * The bio request @bio needs to be split into two bio requests.  The newly
 * created @bio request starts at @new_sector. This split may be required due to
 * hardware limitations such as operation crossing device boundaries in a RAID
 * system.
 */
TRACE_EVENT(block_split,

	TP_PROTO(struct bio *bio, unsigned int new_sector),

	TP_ARGS(bio, new_sector),

	TP_STRUCT__entry(
		__field( dev_t,		dev				)
		__field( sector_t,	sector				)
		__field( sector_t,	new_sector			)
		__array( char,		rwbs,		RWBS_LEN	)
		__array( char,		comm,		TASK_COMM_LEN	)
	),

	TP_fast_assign(
		__entry->dev		= bio_dev(bio);
		__entry->sector		= bio->bi_iter.bi_sector;
		__entry->new_sector	= new_sector;
		blk_fill_rwbs(__entry->rwbs, bio->bi_opf);
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
	),

	TP_printk("%d,%d %s %llu / %llu [%s]",
		  MAJOR(__entry->dev), MINOR(__entry->dev), __entry->rwbs,
		  (unsigned long long)__entry->sector,
		  (unsigned long long)__entry->new_sector,
		  __entry->comm)
);

/**
 * block_bio_remap - map request for a logical device to the raw device
 * @bio: revised operation
 * @dev: original device for the operation
 * @from: original sector for the operation
 *
 * An operation for a logical device has been mapped to the
 * raw block device.
 */
TRACE_EVENT(block_bio_remap,

	TP_PROTO(struct bio *bio, dev_t dev, sector_t from),

	TP_ARGS(bio, dev, from),

	TP_STRUCT__entry(
		__field( dev_t,		dev		)
		__field( sector_t,	sector		)
		__field( unsigned int,	nr_sector	)
		__field( dev_t,		old_dev		)
		__field( sector_t,	old_sector	)
		__array( char,		rwbs,	RWBS_LEN)
	),

	TP_fast_assign(
		__entry->dev		= bio_dev(bio);
		__entry->sector		= bio->bi_iter.bi_sector;
		__entry->nr_sector	= bio_sectors(bio);
		__entry->old_dev	= dev;
		__entry->old_sector	= from;
		blk_fill_rwbs(__entry->rwbs, bio->bi_opf);
	),

	TP_printk("%d,%d %s %llu + %u <- (%d,%d) %llu",
		  MAJOR(__entry->dev), MINOR(__entry->dev), __entry->rwbs,
		  (unsigned long long)__entry->sector,
		  __entry->nr_sector,
		  MAJOR(__entry->old_dev), MINOR(__entry->old_dev),
		  (unsigned long long)__entry->old_sector)
);

/**
 * block_rq_remap - map request for a block operation request
 * @rq: block IO operation request
 * @dev: device for the operation
 * @from: original sector for the operation
 *
 * The block operation request @rq in @q has been remapped.  The block
 * operation request @rq holds the current information and @from hold
 * the original sector.
 */
TRACE_EVENT(block_rq_remap,

	TP_PROTO(struct request *rq, dev_t dev, sector_t from),

	TP_ARGS(rq, dev, from),

	TP_STRUCT__entry(
		__field( dev_t,		dev		)
		__field( sector_t,	sector		)
		__field( unsigned int,	nr_sector	)
		__field( dev_t,		old_dev		)
		__field( sector_t,	old_sector	)
		__field( unsigned int,	nr_bios		)
		__array( char,		rwbs,	RWBS_LEN)
	),

	TP_fast_assign(
		__entry->dev		= disk_devt(rq->q->disk);
		__entry->sector		= blk_rq_pos(rq);
		__entry->nr_sector	= blk_rq_sectors(rq);
		__entry->old_dev	= dev;
		__entry->old_sector	= from;
		__entry->nr_bios	= blk_rq_count_bios(rq);
		blk_fill_rwbs(__entry->rwbs, rq->cmd_flags);
	),

	TP_printk("%d,%d %s %llu + %u <- (%d,%d) %llu %u",
		  MAJOR(__entry->dev), MINOR(__entry->dev), __entry->rwbs,
		  (unsigned long long)__entry->sector,
		  __entry->nr_sector,
		  MAJOR(__entry->old_dev), MINOR(__entry->old_dev),
		  (unsigned long long)__entry->old_sector, __entry->nr_bios)
);

/**
 * blkdev_zone_mgmt - Execute a zone management operation on a range of zones
 * @bio: The block IO operation sent down to the device
 * @nr_sectors: The number of sectors affected by this operation
 *
 * Execute a zone management operation on a specified range of zones. This
 * range is encoded in %nr_sectors, which has to be a multiple of the zone
 * size.
 */
TRACE_EVENT(blkdev_zone_mgmt,

	TP_PROTO(struct bio *bio, sector_t nr_sectors),

	TP_ARGS(bio, nr_sectors),

	TP_STRUCT__entry(
	    __field(  dev_t,	dev		)
	    __field(  sector_t,	sector		)
	    __field(  sector_t, nr_sectors	)
	    __array(  char,	rwbs,	RWBS_LEN)
	),

	TP_fast_assign(
	    __entry->dev	= bio_dev(bio);
	    __entry->sector	= bio->bi_iter.bi_sector;
	    __entry->nr_sectors	= bio_sectors(bio);
	    blk_fill_rwbs(__entry->rwbs, bio->bi_opf);
        ),

	TP_printk("%d,%d %s %llu + %llu",
		  MAJOR(__entry->dev), MINOR(__entry->dev), __entry->rwbs,
		  (unsigned long long)__entry->sector,
		  __entry->nr_sectors)
);

DECLARE_EVENT_CLASS(block_zwplug,

	TP_PROTO(struct request_queue *q, unsigned int zno, sector_t sector,
		 unsigned int nr_sectors),

	TP_ARGS(q, zno, sector, nr_sectors),

	TP_STRUCT__entry(
		__field( dev_t,		dev		)
		__field( unsigned int,	zno		)
		__field( sector_t,	sector		)
		__field( unsigned int,	nr_sectors	)
	),

	TP_fast_assign(
		__entry->dev		= disk_devt(q->disk);
		__entry->zno		= zno;
		__entry->sector		= sector;
		__entry->nr_sectors	= nr_sectors;
	),

	TP_printk("%d,%d zone %u, BIO %llu + %u",
		  MAJOR(__entry->dev), MINOR(__entry->dev), __entry->zno,
		  (unsigned long long)__entry->sector,
		  __entry->nr_sectors)
);

DEFINE_EVENT(block_zwplug, disk_zone_wplug_add_bio,

	TP_PROTO(struct request_queue *q, unsigned int zno, sector_t sector,
		 unsigned int nr_sectors),

	TP_ARGS(q, zno, sector, nr_sectors)
);

DEFINE_EVENT(block_zwplug, blk_zone_wplug_bio,

	TP_PROTO(struct request_queue *q, unsigned int zno, sector_t sector,
		 unsigned int nr_sectors),

	TP_ARGS(q, zno, sector, nr_sectors)
);

#endif /* _TRACE_BLOCK_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

