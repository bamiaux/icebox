#include "os.hpp"

#define FDP_MODULE "os_linux"
#include "core.hpp"
#include "log.hpp"
#include "reader.hpp"
#include "utils/fnview.hpp"

namespace
{
    struct os_offsets
    {
        uint64_t name;
        uint64_t tasks;
        uint64_t mm;
        uint64_t pid;
        uint64_t pgd;
    };

    static const os_offsets linux_4_15_0_39_offsets =
        {
            0xa50,
            0x7a8,
            0x7f8,
            0x8a8,
            0x50,
    };

    struct OsLinux
        : public os::IModule
    {
        OsLinux(core::Core& core);

        // methods
        bool setup();

        // os::IModule
        bool    is_kernel_address   (uint64_t ptr) override;
        bool    can_inject_fault    (uint64_t ptr) override;
        bool    reader_setup        (reader::Reader& reader, proc_t proc) override;

        bool                proc_list           (on_proc_fn on_process) override;
        opt<proc_t>         proc_current        () override;
        opt<proc_t>         proc_find           (std::string_view name) override;
        opt<proc_t>         proc_find           (uint64_t pid) override;
        opt<std::string>    proc_name           (proc_t proc) override;
        bool                proc_is_valid       (proc_t proc) override;
        uint64_t            proc_id             (proc_t proc) override;
        flags_e             proc_flags          (proc_t proc) override;
        void                proc_join           (proc_t proc, os::join_e join) override;
        opt<phy_t>          proc_resolve        (proc_t proc, uint64_t ptr) override;
        opt<proc_t>         proc_select         (proc_t proc, uint64_t ptr) override;
        bool                proc_listen_create  (const on_proc_event_fn& on_proc_event) override;
        bool                proc_listen_delete  (const on_proc_event_fn& on_proc_event) override;

        bool            thread_list         (proc_t proc, on_thread_fn on_thread) override;
        opt<thread_t>   thread_current      () override;
        opt<proc_t>     thread_proc         (thread_t thread) override;
        opt<uint64_t>   thread_pc           (proc_t proc, thread_t thread) override;
        uint64_t        thread_id           (proc_t proc, thread_t thread) override;
        bool            thread_listen_create(const on_thread_event_fn& on_create) override;
        bool            thread_listen_delete(const on_thread_event_fn& on_remove) override;

        bool                mod_list            (proc_t proc, on_mod_fn on_module) override;
        opt<std::string>    mod_name            (proc_t proc, mod_t mod) override;
        opt<span_t>         mod_span            (proc_t proc, mod_t mod) override;
        opt<mod_t>          mod_find            (proc_t proc, uint64_t addr) override;
        bool                mod_listen_load     (const on_mod_event_fn& on_load) override;
        bool                mod_listen_unload   (const on_mod_event_fn& on_unload) override;

        bool                driver_list (on_driver_fn on_driver) override;
        opt<driver_t>       driver_find (std::string_view name) override;
        opt<std::string>    driver_name (driver_t drv) override;
        opt<span_t>         driver_span (driver_t drv) override;

        opt<arg_t>  read_stack  (size_t index) override;
        opt<arg_t>  read_arg    (size_t index) override;
        bool        write_arg   (size_t index, arg_t arg) override;

        void debug_print() override;

        // members
        core::Core& core_;
        os_offsets  members_;
        uint64_t    init_task_addr_;
    };
}

OsLinux::OsLinux(core::Core& core)
    : core_(core)
{
}

bool OsLinux::setup()
{
    members_ = linux_4_15_0_39_offsets;

    // Get this from System.map and deal with KASLR (look at pyrebox)
    init_task_addr_ = 0xffffffff9e612480;

    return true;
}

std::unique_ptr<os::IModule> os::make_linux(core::Core& core)
{
    auto oslinux = std::make_unique<OsLinux>(core);
    if(!oslinux)
        return nullptr;

    const auto ok = oslinux->setup();
    if(!ok)
        return nullptr;

    return oslinux;
}

bool OsLinux::proc_list(on_proc_fn on_process)
{
    const auto proc = core_.os->proc_current();
    if(!proc)
        return false;

    const auto head   = init_task_addr_ + members_.tasks;
    const auto reader = reader::make(core_, *proc);
    for(auto link = reader.read(head); link != head; link = reader.read(*link))
    {
        const auto task_struc = *link - members_.tasks;
        const auto pgd        = reader.read(task_struc + members_.pgd);
        if(!pgd)
        {
            LOG(ERROR, "unable to read task_struct.mm_struct.pgd from {:#x}", task_struc);
            continue;
        }

        const auto err = on_process({task_struc, {*pgd}});
        if(err == WALK_STOP)
            break;
    }
    return true;
}

opt<proc_t> OsLinux::proc_current()
{
    return {};
}

opt<proc_t> OsLinux::proc_find(std::string_view name)
{
    opt<proc_t> found;
    proc_list([&](proc_t proc)
    {
        const auto got = proc_name(proc);
        if(got != name)
            return WALK_NEXT;

        found = proc;
        return WALK_STOP;
    });
    return found;
}

opt<proc_t> OsLinux::proc_find(uint64_t pid)
{
    opt<proc_t> found;
    proc_list([&](proc_t proc)
    {
        const auto got = proc_id(proc);
        if(got != pid)
            return WALK_NEXT;

        found = proc;
        return WALK_STOP;
    });
    return found;
}

opt<std::string> OsLinux::proc_name(proc_t proc)
{
    char buffer[14 + 1];
    const auto reader         = reader::make(core_, proc);
    const auto ok             = reader.read(buffer, proc.id + members_.name, sizeof buffer);
    buffer[sizeof buffer - 1] = 0;
    if(!ok)
        return {};

    const auto name = std::string{buffer};
    if(name.size() < sizeof buffer - 1)
        return name;

    return name;
}

uint64_t OsLinux::proc_id(proc_t proc)
{
    // pid is a uin32_t on linux
    const auto reader = reader::make(core_, proc);
    const auto pid    = reader.le32(proc.id + members_.pid);
    if(!pid)
        return 0;

    return 0x0000000000000000 | *pid;
}

// DON'T USE THESE FUNCTIONS UNTIL YOU REWRITE THEM
bool OsLinux::proc_is_valid(proc_t /*proc*/)
{
    return true;
}

bool OsLinux::is_kernel_address(uint64_t /*ptr*/)
{
    return false;
}

bool OsLinux::can_inject_fault(uint64_t /*ptr*/)
{
    return false;
}

flags_e OsLinux::proc_flags(proc_t /*proc*/)
{
    return FLAGS_NONE;
}

void OsLinux::proc_join(proc_t /*proc*/, os::join_e /*join*/)
{
}

opt<phy_t> OsLinux::proc_resolve(proc_t /*proc*/, uint64_t /*ptr*/)
{
    return {};
}

opt<proc_t> OsLinux::proc_select(proc_t proc, uint64_t /*ptr*/)
{
    return proc;
}

bool OsLinux::proc_listen_create(const on_proc_event_fn& /*on_create*/)
{
    return true;
}

bool OsLinux::proc_listen_delete(const on_proc_event_fn& /*on_remove*/)
{
    return true;
}

bool OsLinux::thread_listen_create(const on_thread_event_fn& /*on_create*/)
{
    return true;
}

bool OsLinux::thread_listen_delete(const on_thread_event_fn& /*on_remove*/)
{
    return true;
}

bool OsLinux::mod_listen_load(const on_mod_event_fn& /*on_load*/)
{
    return true;
}

bool OsLinux::mod_listen_unload(const on_mod_event_fn& /*on_load*/)
{
    return true;
}

bool OsLinux::reader_setup(reader::Reader& reader, proc_t proc)
{
    reader.udtb_ = proc.dtb;
    reader.kdtb_ = proc.dtb;
    return true;
}

bool OsLinux::thread_list(proc_t /*proc*/, on_thread_fn on_thread)
{
    thread_t dummy_thread = {0};
    on_thread(dummy_thread);
    return true;
}

opt<thread_t> OsLinux::thread_current()
{
    return {};
}

opt<proc_t> OsLinux::thread_proc(thread_t /*thread*/)
{
    return {};
}

opt<uint64_t> OsLinux::thread_pc(proc_t /*proc*/, thread_t /*thread*/)
{
    return {};
}

uint64_t OsLinux::thread_id(proc_t /*proc*/, thread_t /*thread*/)
{
    return 0;
}

bool OsLinux::mod_list(proc_t /*proc*/, on_mod_fn on_module)
{
    mod_t dummy_mod = {0, FLAGS_NONE};
    on_module(dummy_mod);
    return true;
}

opt<std::string> OsLinux::mod_name(proc_t /*proc*/, mod_t /*mod*/)
{
    return {};
}

opt<span_t> OsLinux::mod_span(proc_t /*proc*/, mod_t /*mod*/)
{
    return {};
}

opt<mod_t> OsLinux::mod_find(proc_t /*proc*/, uint64_t /*addr*/)
{
    return {};
}

bool OsLinux::driver_list(on_driver_fn on_driver)
{
    driver_t dummy_driver = {0};
    on_driver(dummy_driver);
    return true;
}

opt<driver_t> OsLinux::driver_find(std::string_view /*name*/)
{
    return {};
}

opt<std::string> OsLinux::driver_name(driver_t /*drv*/)
{
    return {};
}

opt<span_t> OsLinux::driver_span(driver_t /*drv*/)
{
    return {};
}

opt<arg_t> OsLinux::read_stack(size_t /*index*/)
{
    return {};
}

opt<arg_t> OsLinux::read_arg(size_t /*index*/)
{
    return {};
}

bool OsLinux::write_arg(size_t /*index*/, arg_t /*arg*/)
{
    return false;
}

void OsLinux::debug_print()
{
}