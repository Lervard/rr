/* -*- Mode: C++; tab-width: 8; c-basic-offset: 8; indent-tabs-mode: t; -*- */

#ifndef TRACE_H_
#define TRACE_H_

#include <signal.h>		/* for _NSIG */
#include <stdint.h>
#include <sys/stat.h>
#include <sys/user.h>

#include <string>
#include <vector>

#include "event.h"
#include "types.h"

class Task;

typedef std::vector<char*> CharpVector;

/* Use this helper to declare a struct member that doesn't occupy
 * space, but the address of which can be taken.  Useful for
 * delimiting continugous chunks of fields without having to hard-code
 * the name of first last fields in the chunk.  (Nested structs
 * achieve the same, but at the expense of unnecessary verbosity.) */
#define STRUCT_DELIMITER(_name) char _name[0]

/**
 * A trace_frame is one "trace event" from a complete trace.  During
 * recording, a trace_frame is recorded upon each significant event,
 * for example a context-switch or syscall.  During replay, a
 * trace_frame represents a "next state" that needs to be transitioned
 * into, and the information recorded in the frame dictates the nature
 * of the transition.
 */
struct trace_frame {
	STRUCT_DELIMITER(begin_event_info);
	uint32_t global_time;
	uint32_t thread_time;
	pid_t tid;
	EncodedEvent ev;
	STRUCT_DELIMITER(end_event_info);

	STRUCT_DELIMITER(begin_exec_info);
	int64_t rbc;
#ifdef HPC_ENABLE_EXTRA_PERF_COUNTERS
	int64_t hw_interrupts;
	int64_t page_faults;
	int64_t insts;
#endif

	struct user_regs_struct recorded_regs;
	STRUCT_DELIMITER(end_exec_info);
};

/* XXX/pedant more accurately called a "mapped /region/", since we're
 * not mapping entire files, necessarily. */
struct mmapped_file {
	/* Global trace time when this region was mapped. */
	uint32_t time;
	int tid;
	/* Did we save a copy of the mapped region in the trace
	 * data? */
	int copied;

	char filename[PATH_MAX];
	struct stat stat;

	/* Bounds of mapped region. */
	void* start;
	void* end;
};

const char* get_trace_path(void);
void open_trace_files(void);
void close_trace_files(void);
void flush_trace_files(void);

/**
 * Log a human-readable representation of |frame| to |out|, including
 * a newline character.
 */
void dump_trace_frame(FILE* out, const struct trace_frame* frame);

/**
 * Recording
 */

void rec_init_trace_files(void);

/**
 * Store |len| bytes of the data in |buf|, which was read from |t|'s
 * address |addr|, to the trace.
 */
void record_data(Task* t, void* addr, ssize_t len, const void* buf);
/** Store |ev| to the trace on behalf of |t|. */
void record_event(Task* t, const Event& ev);
/**
 * Record that the trace will be ending abnormally early, usually
 * because of an interrupting signal.  |t| was the last task known to
 * have run, and it may be nullptr.
 */
void record_trace_termination_event(Task* t);
void record_mmapped_file_stats(struct mmapped_file* file);
/**
 * Return the current global time.  This is approximately the number
 * of events that have been recorded or replayed.  It is exactly the
 * line number within the first trace file (trace_dir/trace_0) of the
 * event that was just recorded or is being replayed.
 *
 * Beware: if there are multiple trace files, this value doesn't
 * directly identify a unique file:line, by itself.
 *
 * TODO: we should either stop creating multiple files, or use an
 * interface like |const char* get_trace_file_coord()| that would
 * return a string like "trace_0:457293".
 */
unsigned int get_global_time(void);
void record_argv_envp(int argc, char* argv[], char* envp[]);
/**
 * Create a unique directory named something like "$(basename
 * exe_path)-$number" in which all trace files will be stored.
 */
void rec_set_up_trace_dir(const char* exe_path);

/**
 * Replaying
 */

/**
 * Read and return the next trace frame.  Succeed or don't return.
 */
void read_next_trace(struct trace_frame* frame);
/**
 * Return nonzero if the next trace frame was read, zero if not.
 */
int try_read_next_trace(struct trace_frame* frame);
void peek_next_trace(struct trace_frame* trace);
void read_next_mmapped_file_stats(struct mmapped_file* file);
void rep_init_trace_files(void);
void* read_raw_data(struct trace_frame* trace, size_t* size_ptr, void** addr);
/**
 * Read the next raw-data record from the trace directly into |buf|,
 * which is of size |buf_size|, without allocating temporary storage.
 * The number of bytes written into |buf| is returned, or -1 if an
 * error occurred.  The tracee address from which this data was
 * recorded is returned in the outparam |rec_addr|.
 */
ssize_t read_raw_data_direct(struct trace_frame* trace,
			     void* buf, size_t buf_size, void** rec_addr);
/**
 * Return the tid of the first thread seen during recording.  Must be
 * called after |init_trace_files()|, and before any calls to
 * |read_next_trace()|.
 */
pid_t get_recorded_main_thread(void);
/**
 * Set the trace directory that will be replayed to |path|.
 */
void rep_set_up_trace_dir(int argc, char** argv);

/**
 * Return the exe image path, arg vector, and environment variables
 * that were recorded, in |exec_image|, |argv|, |envp| respectively.
 *
 * Must be called after |rep_set_up_trace_dir()|.
 */
void load_recorded_env(int* argc, std::string* exec_image,
		       CharpVector* argv, CharpVector* envp);

#endif /* TRACE_H_ */
