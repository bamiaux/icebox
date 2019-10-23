#include "os.hpp"

#define PRIVATE_CORE__
#define FDP_MODULE "core"
#include "core.hpp"
#include "core_private.hpp"
#include "os_private.hpp"
#include "utils/fnview.hpp"

bool os::is_kernel_address(core::Core& core, uint64_t ptr)
{
    return core.os_->is_kernel_address(ptr);
}

bool os::can_inject_fault(core::Core& core, uint64_t ptr)
{
    if(!core.os_)
        return false;

    return core.os_->can_inject_fault(ptr);
}

bool os::reader_setup(core::Core& core, reader::Reader& reader, opt<proc_t> proc)
{
    if(!core.os_)
        return true;

    return core.os_->reader_setup(reader, proc);
}

sym::Symbols& os::kernel_symbols(core::Core& core)
{
    return core.os_->kernel_symbols();
}

opt<os::bpid_t> os::listen_proc_create(core::Core& core, const on_proc_event_fn& on_proc_event)
{
    return core.os_->listen_proc_create(on_proc_event);
}

opt<os::bpid_t> os::listen_proc_delete(core::Core& core, const on_proc_event_fn& on_proc_event)
{
    return core.os_->listen_proc_delete(on_proc_event);
}

opt<os::bpid_t> os::listen_thread_create(core::Core& core, const on_thread_event_fn& on_thread_event)
{
    return core.os_->listen_thread_create(on_thread_event);
}

opt<os::bpid_t> os::listen_thread_delete(core::Core& core, const on_thread_event_fn& on_thread_event)
{
    return core.os_->listen_thread_delete(on_thread_event);
}

opt<os::bpid_t> os::listen_mod_create(core::Core& core, const on_mod_event_fn& on_load)
{
    return core.os_->listen_mod_create(on_load);
}

opt<os::bpid_t> os::listen_drv_create(core::Core& core, const on_drv_event_fn& on_load)
{
    return core.os_->listen_drv_create(on_load);
}

size_t os::unlisten(core::Core& core, bpid_t bpid)
{
    return core.os_->unlisten(bpid);
}

opt<arg_t> os::read_stack(core::Core& core, size_t index)
{
    return core.os_->read_stack(index);
}

opt<arg_t> os::read_arg(core::Core& core, size_t index)
{
    return core.os_->read_arg(index);
}

bool os::write_arg(core::Core& core, size_t index, arg_t arg)
{
    return core.os_->write_arg(index, arg);
}

void os::debug_print(core::Core& core)
{
    return core.os_->debug_print();
}
