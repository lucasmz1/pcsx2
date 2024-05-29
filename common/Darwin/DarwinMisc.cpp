// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#if defined(__APPLE__)

#include "common/Darwin/DarwinMisc.h"
#include "common/HostSys.h"

#include <cstring>
#include <cstdlib>
#include <optional>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <time.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/mach_time.h>
#include <mach/mach_vm.h>
#include <mach/vm_map.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

#include "common/Assertions.h"
#include "common/BitUtils.h"
#include "common/Console.h"
#include "common/Pcsx2Types.h"
#include "common/HostSys.h"
#include "common/Threading.h"
#include "common/WindowInfo.h"

// Darwin (OSX) is a bit different from Linux when requesting properties of
// the OS because of its BSD/Mach heritage. Helpfully, most of this code
// should translate pretty well to other *BSD systems. (e.g.: the sysctl(3)
// interface).
//
// For an overview of all of Darwin's sysctls, check:
// https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man3/sysctl.3.html

// Return the total physical memory on the machine, in bytes. Returns 0 on
// failure (not supported by the operating system).
u64 GetPhysicalMemory()
{
	u64 getmem = 0;
	size_t len = sizeof(getmem);
	int mib[] = {CTL_HW, HW_MEMSIZE};
	if (sysctl(mib, std::size(mib), &getmem, &len, NULL, 0) < 0)
		perror("sysctl:");
	return getmem;
}

static mach_timebase_info_data_t s_timebase_info;
static const u64 tickfreq = []() {
	if (mach_timebase_info(&s_timebase_info) != KERN_SUCCESS)
		abort();
	return (u64)1e9 * (u64)s_timebase_info.denom / (u64)s_timebase_info.numer;
}();

// returns the performance-counter frequency: ticks per second (Hz)
//
// usage:
//   u64 seconds_passed = GetCPUTicks() / GetTickFrequency();
//   u64 millis_passed = (GetCPUTicks() * 1000) / GetTickFrequency();
//
// NOTE: multiply, subtract, ... your ticks before dividing by
// GetTickFrequency() to maintain good precision.
u64 GetTickFrequency()
{
	return tickfreq;
}

// return the number of "ticks" since some arbitrary, fixed time in the
// past. On OSX x86(-64), this is actually the number of nanoseconds passed,
// because mach_timebase_info.numer == denom == 1. So "ticks" ==
// nanoseconds.
u64 GetCPUTicks()
{
	return mach_absolute_time();
}

static std::string sysctl_str(int category, int name)
{
	char buf[32];
	size_t len = sizeof(buf);
	int mib[] = {category, name};
	sysctl(mib, std::size(mib), buf, &len, nullptr, 0);
	return std::string(buf, len > 0 ? len - 1 : 0);
}

static std::optional<u32> sysctlbyname_u32(const char* name)
{
	u32 output;
	size_t output_size = sizeof(output);
	if (0 != sysctlbyname(name, &output, &output_size, nullptr, 0))
		return std::nullopt;
	if (output_size != sizeof(output))
	{
		DevCon.WriteLn("(DarwinMisc) sysctl %s gave unexpected size %zd", name, output_size);
		return std::nullopt;
	}
	return output;
}

std::string GetOSVersionString()
{
	std::string type = sysctl_str(CTL_KERN, KERN_OSTYPE);
	std::string release = sysctl_str(CTL_KERN, KERN_OSRELEASE);
	std::string arch = sysctl_str(CTL_HW, HW_MACHINE);
	return type + " " + release + " " + arch;
}

static IOPMAssertionID s_pm_assertion;

bool Common::InhibitScreensaver(bool inhibit)
{
	if (s_pm_assertion)
	{
		IOPMAssertionRelease(s_pm_assertion);
		s_pm_assertion = 0;
	}

	if (inhibit)
		IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep, kIOPMAssertionLevelOn, CFSTR("Playing a game"), &s_pm_assertion);

	return true;
}

void Threading::Sleep(int ms)
{
	usleep(1000 * ms);
}

void Threading::SleepUntil(u64 ticks)
{
	// This is definitely sub-optimal, but apparently clock_nanosleep() doesn't exist.
	const s64 diff = static_cast<s64>(ticks - GetCPUTicks());
	if (diff <= 0)
		return;

	const u64 nanos = (static_cast<u64>(diff) * static_cast<u64>(s_timebase_info.denom)) / static_cast<u64>(s_timebase_info.numer);
	if (nanos == 0)
		return;

	struct timespec ts;
	ts.tv_sec = nanos / 1000000000ULL;
	ts.tv_nsec = nanos % 1000000000ULL;
	nanosleep(&ts, nullptr);
}

std::vector<DarwinMisc::CPUClass> DarwinMisc::GetCPUClasses()
{
	std::vector<CPUClass> out;

	if (std::optional<u32> nperflevels = sysctlbyname_u32("hw.nperflevels"))
	{
		char name[64];
		for (u32 i = 0; i < *nperflevels; i++)
		{
			snprintf(name, sizeof(name), "hw.perflevel%u.physicalcpu", i);
			std::optional<u32> physicalcpu = sysctlbyname_u32(name);
			snprintf(name, sizeof(name), "hw.perflevel%u.logicalcpu", i);
			std::optional<u32> logicalcpu = sysctlbyname_u32(name);

			char levelname[64];
			size_t levelname_size = sizeof(levelname);
			snprintf(name, sizeof(name), "hw.perflevel%u.name", i);
			if (0 != sysctlbyname(name, levelname, &levelname_size, nullptr, 0))
				strcpy(levelname, "???");

			if (!physicalcpu.has_value() || !logicalcpu.has_value())
			{
				Console.Warning("(DarwinMisc) Perf level %u is missing data on %s cpus!",
					i, !physicalcpu.has_value() ? "physical" : "logical");
				continue;
			}

			out.push_back({levelname, *physicalcpu, *logicalcpu});
		}
	}
	else if (std::optional<u32> physcpu = sysctlbyname_u32("hw.physicalcpu"))
	{
		out.push_back({"Default", *physcpu, sysctlbyname_u32("hw.logicalcpu").value_or(0)});
	}
	else
	{
		Console.Warning("(DarwinMisc) Couldn't get cpu core count!");
	}

	return out;
}

static __ri vm_prot_t MachProt(const PageProtectionMode& mode)
{
	vm_prot_t machmode = (mode.CanWrite()) ? VM_PROT_WRITE : 0;
	machmode |= (mode.CanRead()) ? VM_PROT_READ : 0;
	machmode |= (mode.CanExecute()) ? (VM_PROT_EXECUTE | VM_PROT_READ) : 0;
	return machmode;
}

void* HostSys::Mmap(void* base, size_t size, const PageProtectionMode& mode)
{
	pxAssertMsg((size & (__pagesize - 1)) == 0, "Size is page aligned");
	if (mode.IsNone())
		return nullptr;

#ifdef __aarch64__
	// We can't allocate executable memory with mach_vm_allocate() on Apple Silicon.
	// Instead, we need to use MAP_JIT with mmap(), which does not support fixed mappings.
	if (mode.CanExecute())
	{
		if (base)
			return nullptr;

		const u32 mmap_prot = mode.CanWrite() ? (PROT_READ | PROT_WRITE | PROT_EXEC) : (PROT_READ | PROT_EXEC);
		const u32 flags = MAP_PRIVATE | MAP_ANON | MAP_JIT;
		void* const res = mmap(nullptr, size, mmap_prot, flags, -1, 0);
		return (res == MAP_FAILED) ? nullptr : res;
	}
#endif

	kern_return_t ret = mach_vm_allocate(mach_task_self(), reinterpret_cast<mach_vm_address_t*>(&base), size,
		base ? VM_FLAGS_FIXED : VM_FLAGS_ANYWHERE);
	if (ret != KERN_SUCCESS)
	{
		DEV_LOG("mach_vm_allocate() returned {}", ret);
		return nullptr;
	}

	ret = mach_vm_protect(mach_task_self(), reinterpret_cast<mach_vm_address_t>(base), size, false, MachProt(mode));
	if (ret != KERN_SUCCESS)
	{
		DEV_LOG("mach_vm_protect() returned {}", ret);
		mach_vm_deallocate(mach_task_self(), reinterpret_cast<mach_vm_address_t>(base), size);
		return nullptr;
	}

	return base;
}

void HostSys::Munmap(void* base, size_t size)
{
	if (!base)
		return;

	mach_vm_deallocate(mach_task_self(), reinterpret_cast<mach_vm_address_t>(base), size);
}

void HostSys::MemProtect(void* baseaddr, size_t size, const PageProtectionMode& mode)
{
	pxAssertMsg((size & (__pagesize - 1)) == 0, "Size is page aligned");

	kern_return_t res = mach_vm_protect(mach_task_self(), reinterpret_cast<mach_vm_address_t>(baseaddr), size, false,
		MachProt(mode));
	if (res != KERN_SUCCESS) [[unlikely]]
	{
		ERROR_LOG("mach_vm_protect() failed: {}", res);
		pxFailRel("mach_vm_protect() failed");
	}
}

std::string HostSys::GetFileMappingName(const char* prefix)
{
	// name actually is not used.
	return {};
}

void* HostSys::CreateSharedMemory(const char* name, size_t size)
{
	mach_vm_size_t vm_size = size;
	mach_port_t port;
	const kern_return_t res = mach_make_memory_entry_64(
		mach_task_self(), &vm_size, 0, MAP_MEM_NAMED_CREATE | VM_PROT_READ | VM_PROT_WRITE, &port, MACH_PORT_NULL);
	if (res != KERN_SUCCESS)
	{
		ERROR_LOG("mach_make_memory_entry_64() failed: {}", res);
		return nullptr;
	}

	return reinterpret_cast<void*>(static_cast<uintptr_t>(port));
}

void HostSys::DestroySharedMemory(void* ptr)
{
	mach_port_deallocate(mach_task_self(), static_cast<mach_port_t>(reinterpret_cast<uintptr_t>(ptr)));
}

void* HostSys::MapSharedMemory(void* handle, size_t offset, void* baseaddr, size_t size, const PageProtectionMode& mode)
{
	mach_vm_address_t ptr = reinterpret_cast<mach_vm_address_t>(baseaddr);
	const kern_return_t res = mach_vm_map(mach_task_self(), &ptr, size, 0, baseaddr ? VM_FLAGS_FIXED : VM_FLAGS_ANYWHERE,
		static_cast<mach_port_t>(reinterpret_cast<uintptr_t>(handle)), offset, FALSE,
		MachProt(mode), VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE);
	if (res != KERN_SUCCESS)
	{
		ERROR_LOG("mach_vm_map() failed: {}", res);
		return nullptr;
	}

	return reinterpret_cast<void*>(ptr);
}

void HostSys::UnmapSharedMemory(void* baseaddr, size_t size)
{
	const kern_return_t res = mach_vm_deallocate(mach_task_self(), reinterpret_cast<mach_vm_address_t>(baseaddr), size);
	if (res != KERN_SUCCESS)
		pxFailRel("Failed to unmap shared memory");
}

#ifdef _M_ARM64

void HostSys::FlushInstructionCache(void* address, u32 size)
{
	__builtin___clear_cache(reinterpret_cast<char*>(address), reinterpret_cast<char*>(address) + size);
}

#endif

SharedMemoryMappingArea::SharedMemoryMappingArea(u8* base_ptr, size_t size, size_t num_pages)
	: m_base_ptr(base_ptr)
	, m_size(size)
	, m_num_pages(num_pages)
{
}

SharedMemoryMappingArea::~SharedMemoryMappingArea()
{
	pxAssertRel(m_num_mappings == 0, "No mappings left");

	if (mach_vm_deallocate(mach_task_self(), reinterpret_cast<mach_vm_address_t>(m_base_ptr), m_size) != KERN_SUCCESS)
		pxFailRel("Failed to release shared memory area");
}


std::unique_ptr<SharedMemoryMappingArea> SharedMemoryMappingArea::Create(size_t size)
{
	pxAssertRel(Common::IsAlignedPow2(size, __pagesize), "Size is page aligned");

	mach_vm_address_t alloc;
	const kern_return_t res =
		mach_vm_map(mach_task_self(), &alloc, size, 0, VM_FLAGS_ANYWHERE,
			MEMORY_OBJECT_NULL, 0, false, VM_PROT_NONE, VM_PROT_NONE, VM_INHERIT_NONE);
	if (res != KERN_SUCCESS)
	{
		ERROR_LOG("mach_vm_map() failed: {}", res);
		return {};
	}

	return std::unique_ptr<SharedMemoryMappingArea>(new SharedMemoryMappingArea(reinterpret_cast<u8*>(alloc), size, size / __pagesize));
}

u8* SharedMemoryMappingArea::Map(void* file_handle, size_t file_offset, void* map_base, size_t map_size, const PageProtectionMode& mode)
{
	pxAssert(static_cast<u8*>(map_base) >= m_base_ptr && static_cast<u8*>(map_base) < (m_base_ptr + m_size));

	const kern_return_t res =
		mach_vm_map(mach_task_self(), reinterpret_cast<mach_vm_address_t*>(&map_base), map_size, 0, VM_FLAGS_OVERWRITE,
			static_cast<mach_port_t>(reinterpret_cast<uintptr_t>(file_handle)), file_offset, false,
			MachProt(mode), VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE);
	if (res != KERN_SUCCESS) [[unlikely]]
	{
		ERROR_LOG("mach_vm_map() failed: {}", res);
		return nullptr;
	}

	m_num_mappings++;
	return static_cast<u8*>(map_base);
}

bool SharedMemoryMappingArea::Unmap(void* map_base, size_t map_size)
{
	pxAssert(static_cast<u8*>(map_base) >= m_base_ptr && static_cast<u8*>(map_base) < (m_base_ptr + m_size));

	const kern_return_t res =
		mach_vm_map(mach_task_self(), reinterpret_cast<mach_vm_address_t*>(&map_base), map_size, 0, VM_FLAGS_OVERWRITE,
			MEMORY_OBJECT_NULL, 0, false, VM_PROT_NONE, VM_PROT_NONE, VM_INHERIT_NONE);
	if (res != KERN_SUCCESS) [[unlikely]]
	{
		ERROR_LOG("mach_vm_map() failed: {}", res);
		return false;
	}

	m_num_mappings--;
	return true;
}

#ifdef _M_ARM64

static thread_local int s_code_write_depth = 0;

void HostSys::BeginCodeWrite()
{
	if ((s_code_write_depth++) == 0)
		pthread_jit_write_protect_np(0);
}

void HostSys::EndCodeWrite()
{
	pxAssert(s_code_write_depth > 0);
	if ((--s_code_write_depth) == 0)
		pthread_jit_write_protect_np(1);
}

#endif

#endif
