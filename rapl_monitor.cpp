#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <bcc/BPF.h>
#include <bcc/libbpf.h> // <--- REQUIRED for bpf_update_elem
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <asm/unistd.h>

// -----------------------------------------------------------------------------
// 1. The BPF Kernel Program (Embedded C)
// -----------------------------------------------------------------------------
const std::string BPF_PROGRAM = R"(
#include <uapi/linux/ptrace.h>
#include <linux/sched.h>

// A map to hold the file descriptor for the RAPL counter
BPF_PERF_ARRAY(rapl_counter, 1);

struct data_t {
    u32 pid;
    u64 rapl_val;
    u32 is_pausing; // 1 = Pausing (Switch Out), 0 = Resuming (Switch In)
};
BPF_PERF_OUTPUT(events);

TRACEPOINT_PROBE(sched, sched_switch) {
    u32 pid_target = PID_PLACEHOLDER;
    u32 prev_pid = args->prev_pid;
    u32 next_pid = args->next_pid;

    // CASE 1: Switching OUT (Pause)
    if (prev_pid == pid_target) {
        struct data_t data = {};
        data.pid = prev_pid;
        data.is_pausing = 1;
        data.rapl_val = rapl_counter.perf_read(CUR_CPU_IDENTIFIER);
        events.perf_submit(args, &data, sizeof(data));
    }

    // CASE 2: Switching IN (Resume)
    if (next_pid == pid_target) {
        struct data_t data = {};
        data.pid = next_pid;
        data.is_pausing = 0;
        data.rapl_val = rapl_counter.perf_read(CUR_CPU_IDENTIFIER);
        events.perf_submit(args, &data, sizeof(data));
    }
    return 0;
}
)";

// -----------------------------------------------------------------------------
// 2. Helper: Open RAPL Perf Event
// -----------------------------------------------------------------------------
static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                            int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

int open_rapl_event(int type, int config) {
    struct perf_event_attr pe = {};
    pe.type = type;
    pe.config = config; 
    pe.size = sizeof(struct perf_event_attr);
    pe.disabled = 1;
    pe.exclude_kernel = 1;

    // Monitor CPU 0
    int fd = perf_event_open(&pe, -1, 0, -1, 0);
    if (fd == -1) {
        perror("perf_event_open failed");
    }
    return fd;
}

// -----------------------------------------------------------------------------
// 3. Callback to handle events from Kernel
// -----------------------------------------------------------------------------
void handle_event(void *cb_cookie, void *data, int data_size) {
    struct data_t {
        uint32_t pid;
        uint64_t rapl_val;
        uint32_t is_pausing;
    } *event = static_cast<struct data_t*>(data);

    std::string state = (event->is_pausing) ? "PAUSED (Switch Out)" : "RESUMED (Switch In)";
    
    std::cout << "[Context Switch] " << state 
              << " | RAPL Value: " << event->rapl_val << std::endl;
}
// -----------------------------------------------------------------------------
// 4. Main Application (CORRECTED)
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    if(argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <PID_TO_MONITOR>" << std::endl;
        return 1;
    }
    
    std::string target_pid = argv[1];
    
    // REPLACE PID PLACEHOLDER
    std::string program = BPF_PROGRAM;
    size_t pos = program.find("PID_PLACEHOLDER");
    if (pos != std::string::npos) {
        program.replace(pos, 15, target_pid);
    }

    // INIT BPF
    ebpf::BPF bpf;
    auto init_res = bpf.init(program);
    if (init_res.code() != 0) {
        std::cerr << "BPF Init failed: " << init_res.msg() << std::endl;
        return 1;
    }

    // --- FIXED LINE BELOW ---
    // The macro TRACEPOINT_PROBE(sched, sched_switch) generates a function 
    // named "tracepoint__sched__sched_switch". We must match that name exactly.
    auto attach_res = bpf.attach_tracepoint("sched:sched_switch", "tracepoint__sched__sched_switch");
    
    if (attach_res.code() != 0) {
        std::cerr << "Attach failed: " << attach_res.msg() << std::endl;
        return 1;
    }

    // OPEN RAPL EVENT
    // IMPORTANT: Ensure Type (15) and Config (0x2) match your CPU!
    // Check: /sys/bus/event_source/devices/power/type
    // If this fails, check your specific numbers using: cat /sys/bus/event_source/devices/power/type
    int rapl_fd = open_rapl_event(15, 0x02); 
    if (rapl_fd < 0) return 1;

    // UPDATE BPF MAP
    auto table = bpf.get_table("rapl_counter");
    int map_fd = table.get_fd(); 
    
    int key = 0;       // CPU 0
    int value = rapl_fd; 
    
    if (bpf_update_elem(map_fd, &key, &value, BPF_ANY) != 0) {
        perror("Failed to update BPF map");
        return 1;
    }

    // START POLLING
    std::cout << "Monitoring PID " << target_pid << " for context switches..." << std::endl;
    std::cout << "(Ignore the 'address space' warnings above, they are harmless)" << std::endl;
    
    if (bpf.open_perf_buffer("events", handle_event).code() != 0) {
        std::cerr << "Failed to open perf buffer" << std::endl;
        return 1;
    }

    while (true) {
        bpf.poll_perf_buffer("events");
    }

    return 0;
}