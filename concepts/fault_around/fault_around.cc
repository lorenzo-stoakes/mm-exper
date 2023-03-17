#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <sched.h>
#include <thread>

#include "shared.h"

#define CHECK

#define MAX_NUM_CPUS (256)

// Reduced version
struct vm_area_struct {
	unsigned long vm_start;
	unsigned long vm_end;
	unsigned long vm_pgoff;
};

struct vm_fault {
	unsigned long address;
	struct vm_area_struct vma;
	pgoff_t pgoff;
};

static vm_fault fault_states[MAX_NUM_CPUS] = {};
static unsigned int seeds[MAX_NUM_CPUS] = {};
static std::thread threads[MAX_NUM_CPUS] = {};

std::mutex io_mutex;

static inline unsigned long vma_pages(struct vm_area_struct *vma)
{
	return (vma->vm_end - vma->vm_start) >> PAGE_SHIFT;
}

template<typename T>
void under_io_lock(T&& fn)
{
	std::lock_guard guard(io_mutex);

	fn();
}

// The kernel version of do_fault_around().
std::pair<unsigned long, unsigned long> original_fault_around(struct vm_fault *vmf, unsigned long fault_around_bytes)
{
	unsigned long address = vmf->address, nr_pages, mask;
	pgoff_t start_pgoff = vmf->pgoff;
	pgoff_t end_pgoff;
	int off;

	nr_pages = READ_ONCE(fault_around_bytes) >> PAGE_SHIFT;
	mask = ~(nr_pages * PAGE_SIZE - 1) & PAGE_MASK;

	address = max(address & mask, vmf->vma.vm_start);
	off = ((vmf->address - address) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1);
	start_pgoff -= off;

	/*
	 *
	 *  end_pgoff is either the end of the page table, the end of
	 *  the vma or nr_pages from start_pgoff, depending what is nearest.
	 */
	end_pgoff = start_pgoff -
		((address >> PAGE_SHIFT) & (PTRS_PER_PTE - 1)) +
		PTRS_PER_PTE - 1;
	end_pgoff = min3(end_pgoff, vma_pages(&vmf->vma) + vmf->vma.vm_pgoff - 1,
			start_pgoff + nr_pages - 1);

	return { start_pgoff, end_pgoff };
}

std::pair<unsigned long, unsigned long> do_fault_around_take1(struct vm_fault *vmf, unsigned long fault_around_bytes)
{
	unsigned long nr_pages = fault_around_bytes >> PAGE_SHIFT;
	unsigned long virt_pgoff = vmf->address >> PAGE_SHIFT;
	unsigned long from, to;

	/* Clamp to PTE. */
	from = virt_pgoff - pte_index(vmf->address);
	to = from + PTRS_PER_PTE - 1;

	/* Clamp to VMA. */
	from = max(from, vmf->vma.vm_start >> PAGE_SHIFT);
	to = min(to, (vmf->vma.vm_end >> PAGE_SHIFT) - 1);

	/* Clamp to fault_around_bytes. */
	from = max(from, ALIGN_DOWN(virt_pgoff, nr_pages));
	to = min(to, from + nr_pages - 1);

	return { vmf->pgoff + from - virt_pgoff, vmf->pgoff + to - virt_pgoff };
}

std::pair<unsigned long, unsigned long> do_fault_around(struct vm_fault *vmf, unsigned long fault_around_bytes)
{
	pgoff_t pte_off = pte_index(vmf->address);
	pgoff_t vma_off = vmf->pgoff - vmf->vma.vm_pgoff;
	pgoff_t nr_pages = READ_ONCE(fault_around_bytes) >> PAGE_SHIFT;

	pgoff_t from = max(pte_off - min(pte_off, vma_off), ALIGN_DOWN(pte_off, nr_pages));
	pgoff_t to = min3(from + nr_pages, PTRS_PER_PTE,
			  pte_off + vma_pages(&vmf->vma) - vma_off) - 1;

	return { vmf->pgoff + from - pte_off, vmf->pgoff + to - pte_off };
}

// Get random number between [from, to) * mult and aligned to align.
uint64_t get_random(unsigned cpu, uint64_t from, uint64_t to, uint64_t align = 1, uint64_t mult = 1)
{
	uint64_t delta = rand_r(&seeds[cpu]) % (to - from);

	return ALIGN_DOWN(mult * (from + delta), align);
}

vm_fault& gen_vmf(unsigned cpu)
{
	auto& vmf = fault_states[cpu];
	auto& vma = vmf.vma;

	vma.vm_start = get_random(cpu, 0x700000000UL, 0x800000000UL, 1, PAGE_SIZE);
	vma.vm_end = vma.vm_start + get_random(cpu, 1, 100, 1, PAGE_SIZE);
	vmf.address = get_random(cpu, vma.vm_start, vma.vm_end, PAGE_SIZE);
	vmf.vma.vm_pgoff = get_random(cpu, 0, 100);
	vmf.pgoff = vmf.vma.vm_pgoff + ((vmf.address - vma.vm_start) >> PAGE_SHIFT);

#ifdef CHECK
	if (vmf.vma.vm_start >= vmf.vma.vm_end) {
		under_io_lock([&] {
			std::cout << std::hex << "vm_start=" << vmf.vma.vm_start << ">= ";
			std::cout << "vm_end=" << vmf.vma.vm_end << "\n";
			exit(EXIT_FAILURE);
		});
	}

	if (vmf.address < vmf.vma.vm_start) {
		under_io_lock([&] {
			std::cout << std::hex << "vm_start=" << vmf.vma.vm_start << "> ";
			std::cout << "address=" << vmf.address << "\n";
			exit(EXIT_FAILURE);
		});
	}

	if (vmf.address >= vmf.vma.vm_end) {
		under_io_lock([&] {
			std::cout << std::hex << "vm_end=" << vmf.vma.vm_end << "<= ";
			std::cout << "address=" << vmf.address << "\n";
			exit(EXIT_FAILURE);
		});
	}

	const auto address_offset = (vmf.address - vmf.vma.vm_start) >> PAGE_SHIFT;
	if (vmf.pgoff != address_offset + vmf.vma.vm_pgoff) {
		under_io_lock([&] {
			std::cout << std::hex << "vm_pgoff=" << vmf.vma.vm_pgoff << ", ";
			std::cout << "address=" << vmf.address << ", offset=" <<  address_offset;
			std::cout << ", pgoff=" << vmf.pgoff << ": invalid!\n";
			exit(EXIT_FAILURE);
		});
	}
#endif

	return vmf;
}

void do_test(unsigned cpu)
{
	vm_fault& vmf = gen_vmf(cpu);
	const unsigned long fault_around_bytes =
		rounddown_pow_of_two(get_random(cpu, PAGE_SIZE, PTRS_PER_PTE * PAGE_SIZE));

	const auto [ orig_start_pgoff, orig_end_pgoff ] =
		original_fault_around(&vmf, fault_around_bytes);

	const auto [ start_pgoff, end_pgoff ] =
		     do_fault_around(&vmf, fault_around_bytes);

	if (start_pgoff != orig_start_pgoff || end_pgoff != orig_end_pgoff) {
		under_io_lock([&] {
			std::cout << std::hex << "vm_start=" << vmf.vma.vm_start << ", vm_end=" << vmf.vma.vm_end;
			std::cout << std::dec << ", vm_pgoff=" << vmf.vma.vm_pgoff << ", address=" << std::hex << vmf.address;
			std::cout << std::dec << ", pgoff=" << vmf.pgoff << ", fault_around_bytes=" << fault_around_bytes << "\n";

			std::cout << "MISMATCH: orig = [" << orig_start_pgoff << ", " << orig_end_pgoff << "], ";
			std::cout << "curr = [" << start_pgoff << ", " << end_pgoff << "]\n";

			std::cout.flush();
			//exit(EXIT_FAILURE);
		});
	}
}

void thread_worker(unsigned cpu)
{
	under_io_lock([cpu] {
		std::cout << cpu << " started\n";
	});

	while (true) {
		do_test(cpu);
	}
}


int main()
{
	const unsigned num_cpus = std::thread::hardware_concurrency();

	if (num_cpus > MAX_NUM_CPUS) {
		std::cerr << "System has " << num_cpus << " which exceeds max of " << MAX_NUM_CPUS << "\n";
		return EXIT_FAILURE;
	}

	for (unsigned i = 0; i < num_cpus; i++) {
		seeds[i] = i;

		threads[i] = std::thread([i] {
			cpu_set_t cpuset;

			CPU_ZERO(&cpuset);
			CPU_SET(i, &cpuset);

			if (pthread_setaffinity_np(threads[i].native_handle(),
						   sizeof(cpu_set_t), &cpuset)) {
				perror("setaffinity()");
				exit(EXIT_FAILURE);
			}

			thread_worker(i);
		});
	}

	for (auto& thread : threads) {
		thread.join();
	}

	return EXIT_SUCCESS;
}
